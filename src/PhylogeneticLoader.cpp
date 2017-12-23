/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 10.07.2013
 */

#include "PhylogeneticLoader.hpp"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <cassert>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <limits>
#include <chrono>
#include <condition_variable>
#include <shared_mutex>

#include <inttypes.h>

#include <experimental/filesystem>

#include "btree/btree_set.h"

#include "Taxon.hpp"
#include "ThreadPool.hpp"

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
static HANDLE hConsole;

static inline bool is_terminal()
{
	if (__terminal < 0) {
#if __unix__
		if (isatty(STDOUT_FILENO))
			__terminal = 1;
		else
			__terminal = 0;
#elif _WIN32
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		if (GetFileType(hConsole) == FILE_TYPE_CHAR)
			__terminal = 1;
		else
			__terminal = 0;
#endif
#if _WIN32
		if (__terminal)
		{
			SetConsoleOutputCP(CP_UTF8);
		}
#endif
	}
	return (bool) __terminal;
}

static inline void goto_beginning_of_line()
{
#if __unix__
	printf("\033[G");
#elif _WIN32
	CONSOLE_SCREEN_BUFFER_INFO pBufferInfo;
	GetConsoleScreenBufferInfo(hConsole, &pBufferInfo);
	COORD pos = pBufferInfo.dwCursorPosition;
	pos.X = 0;
	SetConsoleCursorPosition(hConsole, pos);
#endif
}

int main (int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("Need filename to process.\n");
		return 1;
	}

	string file(argv[1]);
	PhylogeneticLoader ldr;

	fs::path path = fs::path(argv[1]);
	string filename = path.stem().string();

#if _WIN32
	// Windows uses wide strings
	printf("%ls: %s\n", path.filename().c_str(), filename.c_str());
#else
	printf("%s: %s\n", path.filename().c_str(), filename.c_str());
#endif

	ldr.parse(file);

	ldr.write(filename);

	ldr.write_timer();

	return 0;
}

void PhylogeneticLoader::parse (const string& file)
{
	timer.start();

	FILE* fp = fopen(file.c_str() ,"r");
	if (!fp)
	{
		printf("Could not open file: %s.\n", file.c_str());
		throw runtime_error("Could not open input file.");
	}

	if (1 != fscanf(fp, "%" SCNu64, &n))
	{
		printf("Expected number of taxas, line: 1\n");
		fclose(fp);
		throw runtime_error("Format error in input file");
	}
	if (1 != fscanf(fp, "%" SCNu64, &m))
	{
		printf("Expected number of haplotypes, line: 2\n");
		fclose(fp);
		throw runtime_error("Format error in input file");
	}
	if (1 != fscanf(fp, "%" SCNu64, &k))
	{
		printf("Expected number of markers, line: 3\n");
		fclose(fp);
		throw runtime_error("Format error in input file");
	}

	printf("Phylogeny with %" PRIu64 " taxas, each %" PRIu64 " haplotypes with %" PRIu64 "-markers. ", n, m, k);
	printf("Possible total: %le\n", pow((double) k, (double) m));

	if (k != 2)
	{
		printf("Cannot Process non binary markers!\n");
		fclose(fp);
		throw runtime_error("Format error in input file");
	}

	partitions0.resize(m);
	partitions1.resize(m);

	try {
		read(fp);
	} catch (exception& e) {
		fclose(fp);
		throw e;
	}
	fclose(fp);

	printf("Found %zu unique taxas. ", nodes.size());
	fflush(stdout);

	weight.resize(m, 1);

	printf("Prepocessing ");
	fflush(stdout);
	preprocess();

	printf("reduced to %" PRIu64 " haplotypes. ", m);
	printf("Possible total: %le\n", pow((double) k, (double) m));

	terminals = nodes.size();

	generate();

	printf("Connecting vertices...\n");

	connect();

	timer.stop();

	printf("Total vertices %zu, total edges %zu\n", nodes.size(), edges.size());
}

void PhylogeneticLoader::preprocess ()
{
	vector<int64_t> action(m, -1);

	for (size_t i = 0; i < m; i++)
	{
		if (partitions0[i].none())
		{
#ifdef DEBUG
			printf("delete column %zu reason: all TRUE\n", i);
#endif
			action[i] = -2;
			continue;
		}

		if (partitions1[i].none())
		{
#ifdef DEBUG
			printf("delete column %zu reason: all FALSE\n", i);
#endif
			action[i] = -2;
			continue;
		}

		if (action[i] == -1)
			for (size_t j = i + 1; j < m; j++)
			{
				if ((partitions0[i] == partitions0[j]) || (partitions0[i] == partitions1[j]))
				{
#ifdef DEBUG
					printf("delete column %zu reason: equivalency w/ column %zu\n", j, i);
#endif
					assert(i < numeric_limits<int64_t>::max());
					action[j] = i;
				}
			}
	}

	auto it0 = partitions0.begin();
	auto it1 = partitions1.begin();
	size_t rem = 0;
	for (size_t c = 0; c < m; c++)
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
	for (size_t c = 0; c < m; c++)
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
	uint64_t generated = 0;
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
		uint64_t last = 0;
		while (true)
		{
			unique_lock<decltype(output.mx)> lock(output.mx);
			output.monitor.wait_for(lock, OUTPUT_TIMEOUT);

			if (is_terminal())
			{
				goto_beginning_of_line();
			}
			{
				shared_lock<decltype(locks.queue)> lock(locks.queue);
				printf("%10" PRIu64 ": queued: %10zu    V/s: %5" PRIu64, generated, p.queued(), (generated-last) * OUTPUT_MULTIPLIER);
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
		for (size_t j = 0; j < m; j++)
		{
			node_type v1(new Taxon(*v.get()));
			//cout << *v << endl;
			v1->flip(j);
			//cout << *v1 << " ";
			{
				shared_lock<decltype(locks.node_set)> lock(locks.node_set);
				//cout << nodes.count(v1) << endl;
				if (nodes.count(v1) > 0)
					continue;
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

	printf("Generated %" PRIu64 " latent taxas. ", generated);
	fflush(stdout);
}

void PhylogeneticLoader::connect ()
{
	uint64_t index = 1;
	for (auto x : nodes)
		x->Index = index++;
	auto i = nodes.begin();
	atomic<uint64_t> counter(0);
	shared_mutex edges_lock;
	mutex output;
	condition_variable output_monitor;
	bool end = false;

	thread output_thread([this, &end, &counter, &output, &index, &edges_lock, &output_monitor] ()
	{
		uint64_t last_e = 0;
		uint64_t last_v = 0;
		uint64_t e;
		double idx = index - 1;
		while (true)
		{
			unique_lock<decltype(output)> lock(output);
			output_monitor.wait_for(lock, OUTPUT_TIMEOUT);
			{
				shared_lock<decltype(edges_lock)> lock_e(edges_lock);
				if (is_terminal())
				{
					goto_beginning_of_line();
				}
				e = edges.size();
			}
			//cout << setw(10) << e << ": " << setw(6) << counter << " / " << setw(6) << index - 1;
			printf("%10" PRIu64 ": %6.2lf%%  E/s: %5" PRIu64 "   V/s: %5" PRIu64,
				e,
				(counter / idx) * 100,
				(e-last_e) * OUTPUT_MULTIPLIER,
				(counter-last_v) * OUTPUT_MULTIPLIER
			);
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
					size_t d = (**i).difference(**m);
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
	for (size_t j = 0; j < m; j++)
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

bool PhylogeneticLoader::isBuneman (const node_type& v, const size_t j) const
{
  auto& p = (v->at(j) ? partitions1[j] : partitions0[j]);

	for (size_t l = 0; l < m; l++)
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
	char filename[name.length() + 4];
	sprintf(filename, "%s.stp", name.c_str());
	
	// open in untranslated mode so windows does not make \r\n in each line
	FILE* stp = fopen(filename, "wb");
	if (!stp)
	{
		printf("Could not open %s for writing.\n", filename);
		throw runtime_error("Could not open output file.");
	}
	write(stp, name);
	fclose(stp);

	sprintf(filename, "%s.map", name.c_str());
	FILE* map = fopen(filename, "wb");
	if (!map)
	{
		printf("Could not open %s for writing.\n", filename);
		throw runtime_error("Could not open output file.");
	}
	writemap(map);
	fclose(map);
}

void PhylogeneticLoader::writemap (FILE* __restrict fp)
{
	fprintf(fp, "%zu\n", nodes.size());
	fprintf(fp, "%" PRIu64 "\n", m);
	fprintf(fp, "%" PRIu64 "\n", k);
	for (auto v : nodes)
	{
		fprintf(fp, "%" PRIu64 "\t", v->Index);
		v->print(fp);
		fprintf(fp, (v->Terminal ? "\tterminal" : ""));
		fputc('\n', fp);
	}
}

void PhylogeneticLoader::write (FILE* __restrict fp, const string& name)
{
	fprintf(fp, "33D32945 STP File, STP Format Version 1.0\n\n");
	fprintf(fp, "SECTION Comment\n");
	fprintf(fp, "Name    \"%s\"\n", name.c_str());
	fprintf(fp, "Creator \"%s\"\n", AUTHOR);
	fprintf(fp, "Program \"" PROGRAM_NAME " " PROGRAM_VERSION "\"\n");
	fprintf(fp, "Problem \"Classical Steiner tree problem in graphs\"\n");
	fprintf(fp, "Remarks \"Converted from Maxmimum Parsimony Phylogeny Estimation Problem\"\n");
	fprintf(fp, "END\n\n");

	fprintf(fp, "SECTION Graph\n");
	fprintf(fp, "Nodes %zu\n", nodes.size());
	fprintf(fp, "Edges %zu\n", edges.size());
	for (auto e : edges)
		fprintf(fp, "E %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", get<0>(e)->Index, get<1>(e)->Index, get<2>(e));
	fprintf(fp, "END\n\n");

	fprintf(fp, "SECTION Terminals\n");
	fprintf(fp, "Terminals %" PRIu64 "\n", terminals);
	for (auto x : nodes)
		if (x->Terminal)
			fprintf(fp, "T %" PRIu64 "\n", x->Index);
	fprintf(fp, "END\n\n");

	fprintf(fp, "SECTION Presolve\n");
	time_t t = time(nullptr);
	fprintf(fp, "Date %s\n", ctime(&t));;
	fprintf(fp, "Time %lf\n", timer.elapsed().getSeconds());
	fprintf(fp, "END\n\n");
	fprintf(fp, "EOF\n");
}

void PhylogeneticLoader::read (FILE* __restrict fp)
{
	// additonal space for '\n' and '\0'
	char taxon[m + 4];
	size_t len;
	// we already read the first 3 lines (n,m,k)
	uint64_t line = 3;
	for (uint64_t i = 0; i < n; i++)
	{
		line++;

		if (feof(fp))
		{
			printf("Unexpected end of file, line: %" PRIu64 "\n", line);
			throw runtime_error("Format error in input file.");
		}

		fgets(taxon, m + 4, fp);
		len = strlen(taxon);

		// ignore empty lines
		if (taxon[0] == '\n' || taxon[0] == '\r')
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
		else if (len < m)
		{
			printf("Unexpected length of taxon, length: %zu, line: %" PRIu64"\n", len, line);
			throw runtime_error("Format error in input file.");
		}

		node_type v(new Taxon(taxon, m));
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
	printf("       Speed up: %5.3lf\n", speedup);
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
