/*
The class PropertyPathBFSSimpleEnum implements a linear iterator for evaluating
 a single property path which has either a strting or an ending point, but not
 both. I.e. (sub)expressions of the form (?x)=[:label*]=>(?y), or
 (?x)=[:label*]=>(?y), where either ?x or ?y are already instantiated. If both
 ?x and ?y are already instantiated the class PropertyPathBFSCheck is used for
 query evaluation.

The BFS search algorihtm is executed from the assigned ending point of the
(sub)expression. This class assumes that QuadModel is used (as most currently
implemented evaluation classes do), in order to be able to access the required
B+ trees.

Since the query pattern uses a regular expression to specify the path constraint,
 it is naturally associated to an automaton equivalent to the regular expression.

Due to the search being guided by the automaton, it can be conceptualized as a
search in the graph consisting of pairs (nodeID,automatonState), where a move
from (nodeID1,automatonState1) to (nodeID2,automatonState2) is possible only if
in our graph there is a connection (nodeID1,type,nodeID2,edge), and
(automatonState1,type,automatonState2) is a transition in the automaton
generated by the regular expression specifying the query.

Elements of the class are:
    - nodes:
        the B+ tree containing all nodes
        (used to determine whether a constant node exists in the graph)
    - type_from_to_edge:
        the B+ tree allowing us to search forward using a type and a from node
    - to_type_from_edge:
        the B+ tree used to traverse edges in reverse
        (from to, knowing type towards from)
    - path_var:
        the ID of the path variable that stores the connected nodes
        (if specified)
    - start:
        the ID of the start node
        (it is possible that this was assigned in a previous iterator that
        piped its results to PropertyPathBFSSimpleEnum; in this case the value
        of this variable is transformed into an object ID in begin())
    - end:
        the ID of the variable storing the end node of the path
        (
        Property path is always evaluated start to end.
        If the patter is of the form (Q1)=[:a*]=>(?x) this is natural.
        A query of the form (?x)=[:a*]=>(Q1) uses the inverse automaton;
        that is, an automaton for (^:a)*, which traverses :a-typed edges
        in reverse. The set of query answers is equivalent to the original
        query (?x)=[:a*]=>(Q1), but the actual query gets rewritten to
        (Q1)=[(^:a)*]=>(?x), and then evaluated.
        )

    - automaton:
        the automaton for the regular expression used to specify the query
    - is_first:
        a boolean value signalling whether the start node of the search is
        already a result to be returned (such as in the case of queries
        (Q1)=[a*]=>(?x), where Q1 is a result whenever present in the graph due to
         the empty path)

    - parent_binding:
        as in all piped iterators, contains the binding passed upwards

    - min_ids:
        search range in the connections B+tree (e.g. in type_from_to_edge)
    - max_ids:
        search range in the connections B+tree

    - visited:
        the set of visited SearchState elements
        (i.e. pairs (nodeID,automatonState) already used in our search)
    - open:
        the queue of SearchState elements we are currently exploring

    - results_found: for statistics
    - bpt_searches: for statistics

The class methods are the same as for any iterator we define in MillenniumDB:
    - begin
    - next
    - reset
    - analyze
    - assign_nulls

Additionally, the following method is used to create the required B+ tree
iterators for BFS search:
    - set_iter(transition, current_state):
        is a transition requires exploring an ":ex" labelled transition in the
        forwards direction, and the current_state contains the node nodeID, a
        B+tree type_from_to_edge iterator is constructed, which has type set to
        ":ex", and from set to nodeID, all to nodes are then iterated over by
        this iterator (together with the edge ID).
*/
#ifndef RELATIONAL_MODEL__PROPERTY_PATH_BFS_SIMPLE_ENUM_H_
#define RELATIONAL_MODEL__PROPERTY_PATH_BFS_SIMPLE_ENUM_H_

#include <array>
#include <memory>
#include <unordered_set>
#include <queue>
#include <variant>

#include "base/binding/binding_id_iter.h"
#include "base/parser/logical_plan/op/property_paths/path_automaton.h"
#include "relational_model/models/quad_model/quad_model.h"
#include "relational_model/execution/binding_id_iter/property_paths/search_state.h"
#include "relational_model/execution/binding_id_iter/scan_ranges/scan_range.h"
#include "storage/index/bplus_tree/bplus_tree.h"

/*
PropertyPathBFSSimpleEnum enumerate all nodes that can be reached from start with
a specific path using the classic implementation of BFS algorithm.
*/

class PropertyPathBFSSimpleEnum : public BindingIdIter {
    using Id = std::variant<VarId, ObjectId>;

private:
    // Attributes determined in the constuctor
    ThreadInfo*   thread_info;
    BPlusTree<1>& nodes;
    BPlusTree<4>& type_from_to_edge;  // Used to search foward
    BPlusTree<4>& to_type_from_edge;  // Used to search backward
    VarId         path_var;
    Id            start;
    VarId         end;
    PathAutomaton automaton;

    // Attributes determined in begin
    BindingId* parent_binding;
    bool is_first;

    // Ranges to search in BPT. They are not local variables because some positions are reused.
    std::array<uint64_t, 4> min_ids;
    std::array<uint64_t, 4> max_ids;

    // Structs for BFS
    std::unordered_set<SearchState, SearchStateHasher> visited;
    std::queue<SearchState> open;

    // Statistics
    uint_fast32_t results_found = 0;
    uint_fast32_t bpt_searches = 0;

    // Constructs iter according to transition
    std::unique_ptr<BptIter<4>> set_iter(const TransitionId& transition, const SearchState& current_state);

public:
    PropertyPathBFSSimpleEnum(ThreadInfo*   thread_info,
                              BPlusTree<1>& nodes,
                              BPlusTree<4>& type_from_to_edge,
                              BPlusTree<4>& to_type_from_edge,
                              VarId         path_var,
                              Id            start,
                              VarId         end,
                              PathAutomaton automaton);
    ~PropertyPathBFSSimpleEnum() = default;

    void analyze(std::ostream& os, int indent = 0) const override;
    void begin(BindingId& parent_binding) override;
    void reset() override;
    void assign_nulls() override;
    bool next() override;
};

#endif // RELATIONAL_MODEL__PROPERTY_PATH_BFS_SIMPLE_ENUM_H_
