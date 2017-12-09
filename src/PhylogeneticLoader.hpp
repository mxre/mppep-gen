/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 10.07.2013
 */

#ifndef PHYLOGENETICLOADER_HPP_
#define PHYLOGENETICLOADER_HPP_

#include <deque>
#include <vector>
#include <istream>
#include <ostream>
#include <string>
#include <memory>
#include <utility>
#include <boost/dynamic_bitset.hpp>

#include "btree/btree_set.h"
#include "Timer.hpp"
#include "Taxon.hpp"

#define PROGRAM_NAME "Phylogeny Converter"
#define PROGRAM_VERSION "1.0"

class PhylogeneticLoader
{
public:
	PhylogeneticLoader ();
	virtual ~PhylogeneticLoader ();

	/// Parse Function
	void parse (std::istream&);
	void write (std::string);

	/// Output timer statistics
	void write_timer();

private:
	/// Data type for nodes
	typedef std::shared_ptr<Taxon> node_type;

	Timer timer;

	/// Unwrap shared_ptr for comparisons
	struct less
	{
	public:
		inline bool operator() (const node_type lhs, const node_type rhs) const noexcept
		{
			return *lhs < *rhs;
		}
	};

	/// Set of nodes
	typedef btree::btree_set<node_type, less> node_set;
	/// Data type for edges
	typedef std::tuple<node_type, node_type, int> edge_type;
	/// List of generated edges
	typedef std::deque<edge_type> edge_list;

	/// Number of Taxas in input matrix
	int n;
	/// Length of each Taxon
	int m;
	/// Number of different values (has to be 2)
	int k;

	std::vector<int> weight;

	/// Taxon from the input
	int terminals;

	node_set nodes;
	edge_list edges;

	/// partition data for Buneman-Graph 0 blocks
	std::vector<boost::dynamic_bitset<>> partitions0;
	/// partition data for Buneman-Graph 1 blocks
	std::vector<boost::dynamic_bitset<>> partitions1;

	/// read the input file
	void read (std::istream&);
	/// write output steiner tree in stp format
	void write (std::ostream&, const std::string&);
	/// write mapping information (to reconstruct original Phylogeny)
	void writemap (std::ostream&);

	/// generate the Buneman-Graph
	void generate ();
	/// Check the Buneman condition for a given node, j is the bit that changed
	bool isBuneman (const node_type&, const u_int j) const;
	/// insert a node into the Buneman data structure, for initialization
	void insertBuneman (const node_type&);

	/// Generate the edges
	void connect();
	/// Row reduction
	void preprocess();

};

#endif /* PHYLOGENETICLOADER_HPP_ */
