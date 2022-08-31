#include "duckdb/execution/operator/helper/physical_limit.hpp"
#include "duckdb/execution/operator/helper/physical_streaming_limit.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/planner/operator/logical_limit.hpp"

namespace duckdb {

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalLimit &op) {
	D_ASSERT(op.children.size() == 1);

	auto plan = CreatePlan(*op.children[0]);
	auto &config = DBConfig::GetConfig(context);
	bool plan_preserves_order = plan->AllOperatorsPreserveOrder();

	unique_ptr<PhysicalOperator> limit;

	if (op.force_stream_and_no_parallel) {
		limit = make_unique<PhysicalStreamingLimit>(op.types, (idx_t)op.limit_val, op.offset_val, move(op.limit),
		                                            move(op.offset), op.estimated_cardinality, false);
	} else if (!config.options.preserve_insertion_order || !plan_preserves_order) {
		// use parallel streaming limit if insertion order is not important
		limit = make_unique<PhysicalStreamingLimit>(op.types, (idx_t)op.limit_val, op.offset_val, move(op.limit),
		                                            move(op.offset), op.estimated_cardinality, true);
	} else {
		// maintaining insertion order is important
		bool all_sources_support_batch_index = plan->AllSourcesSupportBatchIndex();

		if (all_sources_support_batch_index) {
			// source supports batch index: use parallel batch limit
			limit = make_unique<PhysicalLimit>(op.types, (idx_t)op.limit_val, op.offset_val, move(op.limit),
			                                   move(op.offset), op.estimated_cardinality);
		} else {
			// source does not support batch index: use a non-parallel streaming limit
			limit = make_unique<PhysicalStreamingLimit>(op.types, (idx_t)op.limit_val, op.offset_val, move(op.limit),
			                                            move(op.offset), op.estimated_cardinality, false);
		}
	}

	limit->children.push_back(move(plan));
	return limit;
}

} // namespace duckdb
