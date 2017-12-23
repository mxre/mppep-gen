/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 10.07.2013
 */

#include "PhylogeneticLoader.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <tuple>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <experimental/filesystem>

#include <shared_mutex>

#include "btree/btree_set.h"

#include "Taxon.hpp"
#include "ThreadPool.hpp"
#include "stacktrace.h"

#if __unix__
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#endif

#define OUTPUT_TIMEOUT std::chrono::milliseconds(250)
#define OUTPUT_MULTIPLIER 4

using namespace std;
namespace fs = std::experimental::filesystem;

static int __terminal = -1;

static inline bool is_terminal()
{
	if (__terminal < 0) {
#if __unix__
		if (isatty(STDOUT_FILENO))
			__terminal = 1;
		else
			__terminal = 0;
#elif _WIN32
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (GetFileType(hOut) == FILE_TYPE_CHAR)
			__terminal = 1;
		else
			__terminal = 0;
#endif
#if _WIN32
		if (__terminal) {
			DWORD mode = 0;
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			GetConsoleMode(hConsole, &mode);
			if (!SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
				__terminal = 0;
			}
			SetConsoleOutputCP(CP_UTF8);
		}
#endif
	}
	return (bool) __terminal;
}

int main (int argc, char* argv[])
{
	register_handler(argv);

	if (argc != 2)
	{
		printf("Need file to process.\n");
		return 1;
	}

	ifstream file(argv[1]);

	if (!file.good())
	{
		printf("Error opening file: %s\n", argv[1]);
		return 2;
	}

	PhylogeneticLoader ldr;

	fs::path path = fs::path(argv[1]);
	cout << path.filename() << ": ";
	string filename = path.stem().string();
	cout << filename << endl;

	ldr.parse(file);
	file.close();

	ldr.write(filename);

	ldr.write_timer();

	return 0;
}

void PhylogeneticLoader::parse (istream& is)
{
	timer.start();

	is >> n;
	is >> m;
	is >> k;

	printf("Phylogeny with %d taxas, each %d haplotypes with %d-markers. ", n, m, k);
	printf("Possible total: %lf.\n", pow((long) k, (long) m));

	if (k != 2)
	{
		printf("Cannot Process non binary markers!\n");
		return;
	}

	partitions0.resize(m);
	partitions1.resize(m);

	read(is);

	printf("Found %u unique taxas. ", nodes.size());
	fflush(stdout);

	weight.resize(m, 1);

	printf("Prepocessing ");
	fflush(stdout);
	preprocess();

	printf("reduced to %d haplotypes. ", m);
	printf("Possible total: %lf.\n", pow((long) k, (long) m));

	terminals = nodes.size();

	generate();

	printf("Connecting vertices...\n");

	connect();

	timer.stop();

	printf("Total vertices %u, total edges %u\n", nodes.size(), edges.size());
}

void PhylogeneticLoader::preprocess ()
{
	vector<int> action(m, -1);

	for (int i = 0; i < m; i++)
	{
		if (partitions0[i].none())
		{
#ifdef DEBUG
			printf("delete column %d reason: all TRUE\n", i);
#endif
			action[i] = -2;
			continue;
		}

		if (partitions1[i].none())
		{
#ifdef DEBUG
			printf("delete column %d reason: all FALSE\n", i);
#endif
			action[i] = -2;
			continue;
		}

		if (action[i] == -1)
			for (int j = i + 1; j < m; j++)
			{
				if ((partitions0[i] == partitions0[j]) || (partitions0[i] == partitions1[j]))
				{
#ifdef DEBUG
					printf("delete column %d reason: equivalency w/ column %d\n", j, i);
#endif
					action[j] = i;
				}
			}
	}

	auto it0 = partitions0.begin();
	auto it1 = partitions1.begin();
	int rem = 0;
	for (int c = 0; c < m; c++)
	{

		if (action[c] == -2)
		{
			it0 = partitions0.erase(it0);
			it1 = partitions1.erase(it1);
			for (node_type n : nodes)
			{
				n->remove(c - rem);
			}
			rem++;
		}
		else if (action[c] >= 0)
		{
			weight[action[c]]++;
			it0 = partitions0.erase(it0);
			it1 = partitions1.erase(it1);
			for (node_type n : nodes)
			{
				n->remove(c - rem);
			}
			rem++;
		}
		else
		{
			it0++;
			it1++;

		}
	}

	// Erase the weight vector in a second pass to keep the action references
	auto w = weight.begin();
	for (int c = 0; c < m; c++)
	{
		if (action[c] == -2 || action[c] >= 0)
		{
			w = weight.erase(w);
		}
		else
		{
			w++;
		}
	}

	for (node_type n : nodes)
	{
		n->resize();
	}

	m = count_if(action.begin(), action.end(), [] (const int a)
	{
		return (a == -1);
	});

	//timer.stop();

}

void PhylogeneticLoader::generate ()
{
	long generated = 0;
	struct lock_t
	{
		shared_mutex buneman;
		shared_mutex queue;
		shared_mutex node_set;
		condition_variable_any empty;
	};
	lock_t locks;
	deque<node_type> queue;

	struct output_t
	{
		mutex mx;
		condition_variable monitor;
		bool condition = false;
	};
	output_t output;
	for (auto v : nodes)
	{
		queue.push_back(v);
		//cout << *v << endl;
	}
	//queue.push_back(*nodes.begin());
	//cout << **nodes.begin() << endl;

	int pool_threads = 0; //m > 100 ? 0 : 1;
	ThreadPool p(pool_threads);

	thread output_thread([&output, &locks, &p, &generated] ()
	{
		long last = 0;
		while (true)
		{
			unique_lock<decltype(output.mx)> lock(output.mx);
			output.monitor.wait_for(lock, OUTPUT_TIMEOUT);

			if (is_terminal())
			{
				printf("\033[G\033[K");
				fflush(stdout);
			}
			{
				shared_lock<decltype(locks.queue)> lock(locks.queue);
				printf("%10d: queued: %10u    V/s: %5d", generated, p.queued(), (generated-last) * OUTPUT_MULTIPLIER);
			}
			last = generated;
			if (is_terminal())
				fflush(stdout);
			else
				printf("\n");

			if (output.condition)
				break;
		}
	});

	const function<void (node_type)> expand = [this, &locks, &queue, &generated, &p] (const node_type &v)
	{
		if (v.get() == nullptr) return;
		for (unsigned int j = 0; j < m; j++)
		{
			node_type v1(new Taxon(*v.get()));
			//cout << *v << endl;
			v1->flip(j);
			//cout << *v1 << " ";
			{
				shared_lock<decltype(locks.node_set)> lock(locks.node_set);
				//cout << nodes.count(v1) << endl;
				if (nodes.count(v1) > 0) continue;
			}
			shared_lock<decltype(locks.buneman)> lock(locks.buneman);
			bool b = isBuneman(v1, j);
			//cout << (b ? "OK" : "--") << endl;
			lock.unlock();
			if (b)
			{
				{
					unique_lock<decltype(locks.node_set)> lock(locks.node_set);
					nodes.insert(v1);
				}
				{
					unique_lock<decltype(locks.buneman)> lock(locks.buneman);
					insertBuneman(v1);
				}
				{
					unique_lock<decltype(locks.queue)> lock(locks.queue);
					queue.push_back(v1);
					generated++;
				}
			}
		}
		locks.empty.notify_one();
	};

	do
	{
		bool stop = false;
		node_type v;
		{
			unique_lock<decltype(locks.queue)> lock(locks.queue);
			while (queue.empty())
			{
				locks.empty.wait(lock);
				if (queue.empty() && p.queued() == 0)
				{
					stop = true;
					break;
				}
			}
			if (stop)
				break;
			v = queue.front();
			queue.pop_front();
			p.enqueue<void>([v, &expand] ()
			{
				expand(v);
			});
		}
	}
	while (true);

	p.shutdown();
	{
		unique_lock<decltype(output.mx)> lock(output.mx);
		output.condition = true;
		output.monitor.notify_one();
	}
	output_thread.join();
	printf("\n");

	printf("Generated %d latent taxas. ", generated);
	fflush(stdout);
}

void PhylogeneticLoader::connect ()
{
	long index = 1;
	for (auto x : nodes)
		x->Index = index++;
	auto i = nodes.begin();
	atomic<long> counter(0);
	shared_mutex edges_lock;
	mutex output;
	condition_variable output_monitor;
	bool end = false;

	thread output_thread([this, &end, &counter, &output, &index, &edges_lock, &output_monitor] ()
	{
		long last_e = 0;
		long last_v = 0;
		long e;
		double idx = index - 1;
		while (true)
		{
			unique_lock<decltype(output)> lock(output);
			output_monitor.wait_for(lock, OUTPUT_TIMEOUT);
			{
				shared_lock<decltype(edges_lock)> lock_e(edges_lock);
				if (is_terminal())
				{
					printf("\033[G\033[K");
					fflush(stdout);
				}
				e = edges.size();
			}
			//cout << setw(10) << e << ": " << setw(6) << counter << " / " << setw(6) << index - 1;
			printf("%10d: %6.2f%%", e, (counter / idx) * 100);
			printf("  E/s: %5d  V/s: %5d", (e-last_e) * OUTPUT_MULTIPLIER, (counter-last_v) * OUTPUT_MULTIPLIER);
			last_e = e;
			last_v = counter;
			if (is_terminal())
				fflush(stdout);
			else
				printf("\n");

			if (end)
				break;
		}
});

	ThreadPool p(0);
	while (i != nodes.end())
	{
		p.enqueue<void>([this, &edges_lock, i, &counter] ()
		{
			auto m = i;
			m++;
			while (m != nodes.end())
			{
				if ((**i).distance(**m) == 1)
				{
					int d = (**i).difference(**m);
					{
						unique_lock<decltype(edges_lock)> l(edges_lock);
						edges.emplace_back(*i, *m, weight[d]);
					}
				}
				m++;
			}

			counter++;

		});
		i++;
	}

	p.shutdown();
	{
		unique_lock<decltype(output)> lock(output);
		end = true;
		output_monitor.notify_one();
	}
	output_thread.join();
	cout << endl;
}

void PhylogeneticLoader::insertBuneman (const node_type& v)
{
	for (int j = 0; j < m; j++)
		if (v->at(j))
		{
			partitions1[j].push_back(true);
			partitions0[j].push_back(false);
		}
		else
		{
			partitions1[j].push_back(false);
			partitions0[j].push_back(true);
		}
}

bool PhylogeneticLoader::isBuneman (const node_type& v, const unsigned int j) const
{
  auto& p = (v->at(j) ? partitions1[j] : partitions0[j]);

	for (int l = 0; l < m; l++)
		if (l == j)
			continue;
		else if (!p.intersects(v->at(l) ? partitions1[l] : partitions0[l]))
		return false;
    /*for (int j = 0; j < m; j++)
    		for (int l = j + 1; l < m; l++)
    			if (!(v->at(j) ? partitions1[j] : partitions0[j]).intersects(v->at(l) ? partitions1[l] : partitions0[l]))
    				return false;*/
	return true;
}

void PhylogeneticLoader::write (const string& name)
{
	stringstream stp_name;
	stringstream map_name;
	stp_name << name << ".stp";
	map_name << name << ".map";
	ofstream stp(stp_name.str());
	ofstream map(map_name.str());
	write(stp, name);
	writemap(map);
	stp.close();
	map.close();
}

void PhylogeneticLoader::writemap (ostream& os)
{
	os << nodes.size() << endl;
	os << m << endl;
	os << k << endl;
	for (auto v : nodes)
	{
		os << v->Index << "\t" << *v << (v->Terminal ? "\tterminal" : "") << endl;
	}
}

void PhylogeneticLoader::write (ostream& os, const string& name)
{
	os << "33D32945 STP File, STP Format Version 1.0\n" << endl;
	os << "SECTION Comment" << endl;
	os << "Name    \"" << name << "\"" << endl;
	os << "Creator \"Max Resch\"" << endl;
	os << "Program \"" << PROGRAM_NAME << " " << PROGRAM_VERSION << "\"" << endl;
	os << "Problem \"Classical Steiner tree problem in graphs\"" << endl;
	os << "Remarks \"Converted from Maxmimum Parsimony Phylogeny Estimation Problem\"" << endl;
	os << "END\n" << endl;

	os << "SECTION Graph" << endl;
	os << "Nodes " << nodes.size() << endl;
	os << "Edges " << edges.size() << endl;
	for (auto e : edges)
		os << "E " << get<0>(e)->Index << " " << get<1>(e)->Index << " " << get<2>(e) << endl;
	os << "END\n" << endl;

	os << "SECTION Terminals" << endl;
	os << "Terminals " << terminals << endl;
	for (auto x : nodes)
		if (x->Terminal)
			os << "T " << x->Index << endl;
	os << "END\n" << endl;

	os << "SECTION Presolve" << endl;
	time_t t = time(nullptr);
	os << "Date " << ctime(&t);
	os << "Time " << timer.elapsed().getSeconds() << endl;
	os << "END\n" << endl;
	os << "EOF" << endl;
}

void PhylogeneticLoader::read (istream& is)
{
	string taxon;
	// we already read the first 3 lines (n,m,k)
	int line = 3;
	for (int i = 0; i < n; i++)
	{
		line++;

		if (!is.good())
		{
			printf("Unexpected end of file, line: %d\n", line);
			return;
		}

		is >> taxon;

		// ignore empty lines
		if (taxon.length() == 0)
		{
			i--;
			continue;
		}
		// ignore comments
		else if (taxon[0] == '#')
		{
			i--;
			continue;
		}
		// exit if we encounter a faulty line
		else if (taxon.length() != m)
		{
			printf("Unexpected length of taxon, length: %u, line: %d\n", taxon.length(), line);
			return;
		}

		node_type v(new Taxon(taxon));
		//bool inserted;
		//decltype(nodes)::iterator it;
		//tie(it, inserted) = nodes.insert(v);

		auto inserted = nodes.insert(v);
		if (get<1>(inserted))
			insertBuneman(v);
	}
}

void PhylogeneticLoader::write_timer ()
{
	double wall = chrono::duration_cast<seconds>(timer.elapsed().wall).count();
	double user = chrono::duration_cast<seconds>(timer.elapsed().user).count();
	double speedup = user / wall;
	printf("Wall Time: %5.3lfs", wall);
	printf("       CPU Time: %5.3lfs", user);
	printf("       Speed up: %5.3lf\n");
}

PhylogeneticLoader::PhylogeneticLoader ()
{
	n = 0;
	m = 0;
	k = 0;
	terminals = 0;
}

PhylogeneticLoader::~PhylogeneticLoader ()
{
}
