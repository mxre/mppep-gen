/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 06.11.2013
 */

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <tuple>

using namespace std;

tuple<string, string, string> parse_identifier (const string& s)
{
	int i = s.find('|') - 1;
	string id = s.substr(1, i);
	string num = s.substr(i + 2, s.find('|', i + 2) - i - 2);
	string desc = s.substr(s.find(' ') + 1, string::npos);

	return make_tuple(id, num, desc);
}

int parse_line (ostream& out, const string& s)
{
	int count = 0;
	for (char c : s)
	{
		switch (c)
		{
		case 'T':
			out << "0001";
			break;
		case 'G':
			out << "0010";
			break;
		case 'C':
			out << "0100";
			break;
		case 'A':
			out << "0000";
			break;
		default:
			out << "0000";
			break;
		}
		count++;
	}

	return count;
}

int main (int argc, char* argv[])
{
	if (argc != 2)
		return 1;

	string filename(argv[1]);

	ifstream in(filename);
	auto bail = [&in] (const string& message) -> int
	{	in.close();
		cout << message << endl;
		return 1;
	};

	if (!in.good())
		return bail("Could not open file");

	string s;

	getline(in, s);
	if (s[0] != '>')
		return bail("File does not begin with a '>'");

	string id, num, desc;
	tie(id, num, desc) = parse_identifier(s);
	cout << id << "-" << num << ": " << desc << endl;

	stringstream outname;
	outname << id << "-" << num << ".txt";
	ofstream out(outname.str());
	if (!out.good())
		return bail("Could not write output file");

	long t = 0;
	long max_len = 0;
	long len = 16578;
	//tuple<string,string,string> last_id;
	while (!in.eof())
	{
		getline(in, s);
		//cout << s << endl;
		if (s[0] == '>')
		{
			if (len > t)
			{
				//cout << get<0>(last_id) << "-" << get<1>(last_id) << ": " << get<2>(last_id) << endl;
				//cout << t << endl;
				for (int i = 0; i < len - t; i++)
				{
					out << "0000";
				}
			}
			if (t > max_len)
			{
				max_len = t;
			}

			out << endl;
			t = 0;
			//last_id = parse_identifier(s);
		}
		else
		{
			t += parse_line(out, s);
		}
	}
	if (len > t)
	{
		//cout << get<0>(last_id) << "-" << get<1>(last_id) << ": " << get<2>(last_id) << endl;
		//cout << t << endl;
		for (int i = 0; i < len - t; i++)
		{
			out << "0000";
		}
	}
	if (t > max_len)
	{
		max_len = t;
	}
	cout << max_len << "  " << max_len * 4L << endl;
	out << endl;
	out.close();
	in.close();
	return 0;
}
