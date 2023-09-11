#include "duckdb/optimizer/statistics_propagator.hpp"
#include "duckdb/planner/operator/logical_cross_product.hpp"

namespace duckdb {

unique_ptr<NodeStatistics> StatisticsPropagator::PropagateStatistics(LogicalCrossProduct &cp,
                                                                     unique_ptr<LogicalOperator> *node_ptr) {
	// first propagate statistics in the child node
	auto left_stats = PropagateStatistics(cp.children[0]);
	auto right_stats = PropagateStatistics(cp.children[1]);
	if (cp.children[0]->type == LogicalOperatorType::LOGICAL_EMPTY_RESULT ||
	    cp.children[1]->type == LogicalOperatorType::LOGICAL_EMPTY_RESULT) {
		ReplaceWithEmptyResult(*node_ptr);
		return nullptr;
	}
	if (!left_stats || !right_stats) {
		return nullptr;
	}
	MultiplyCardinalities(left_stats, *right_stats);
	return left_stats;
}

} // namespace duckdb
