#include "duckdb/core_functions/scalar/list_functions.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/swap.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/function/scalar/nested_functions.hpp"
#include "duckdb/function/scalar/string_functions.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"

namespace duckdb {

template <typename INDEX_TYPE>
static int CalculateSliceLength(idx_t begin, idx_t end, INDEX_TYPE step, bool svalid) {
	if (step < 0) {
		step = abs(step);
	}
	if (step == 0 && svalid) {
		throw InvalidInputException("Slice step cannot be zero");
	}
	if (step == 1) {
		return end - begin;
	} else if (static_cast<idx_t>(step) >= (end - begin)) {
		return 1;
	}
	if ((end - begin) % step != 0) {
		return (end - begin) / step + 1;
	}
	return (end - begin) / step;
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
INDEX_TYPE ValueLength(const INPUT_TYPE &value) {
	return 0;
}

template <>
int64_t ValueLength(const list_entry_t &value) {
	return value.length;
}

template <>
int64_t ValueLength(const string_t &value) {
	return LengthFun::Length<string_t, int64_t>(value);
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
bool ClampIndex(INDEX_TYPE &index, const INPUT_TYPE &value, const INDEX_TYPE length) {
	if (index < 0) {
		if (index < -length) {
			return false;
		}
		index = length + index;
		return true;
	} else if (index > length) {
		index = length;
	}
	return true;
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
static bool ClampSlice(const INPUT_TYPE &value, INDEX_TYPE &begin, INDEX_TYPE &end) {
	// Clamp offsets
	if (begin == INT64_MIN) {
		return false;
	}
	begin = (begin > 0) ? begin - 1 : begin;
	const auto length = ValueLength<INPUT_TYPE, INDEX_TYPE>(value);
	if (begin < 0 && -begin > length && end < 0 && -end > length) {
		begin = 0;
		end = 0;
		return true;
	}
	if (!ClampIndex(begin, value, length) || !ClampIndex(end, value, length)) {
		return false;
	}
	end = MaxValue<INDEX_TYPE>(begin, end);

	return true;
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
INPUT_TYPE SliceValue(Vector &result, INPUT_TYPE input, INDEX_TYPE begin, INDEX_TYPE end) {
	return input;
}

template <>
list_entry_t SliceValue(Vector &result, list_entry_t input, int64_t begin, int64_t end) {
	input.offset += begin;
	input.length = end - begin;
	return input;
}

template <>
string_t SliceValue(Vector &result, string_t input, int64_t begin, int64_t end) {
	// one-based - zero has strange semantics
	return SubstringFun::SubstringUnicode(result, input, begin + 1, end - begin);
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
INPUT_TYPE SliceValueWithSteps(Vector &result, SelectionVector &sel, INPUT_TYPE input, INDEX_TYPE begin, INDEX_TYPE end,
                               INDEX_TYPE step, idx_t &sel_idx) {
	return input;
}

template <>
list_entry_t SliceValueWithSteps(Vector &result, SelectionVector &sel, list_entry_t input, int64_t begin, int64_t end,
                                 int64_t step, idx_t &sel_idx) {
	if (end - begin == 0) {
		input.length = 0;
		input.offset = sel_idx;
		return input;
	}
	input.length = CalculateSliceLength(begin, end, step, true);
	idx_t child_idx = input.offset + begin;
	if (step < 0) {
		child_idx = input.offset + end - 1;
	}
	input.offset = sel_idx;
	for (idx_t i = 0; i < input.length; i++) {
		sel.set_index(sel_idx, child_idx);
		child_idx += step;
		sel_idx++;
	}
	return input;
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
static void CheckBeginAndEnd(INDEX_TYPE &begin, INDEX_TYPE &end, INDEX_TYPE step, INPUT_TYPE &sliced) {
	if (step < 0) {
		swap(begin, end);
	}

	if (begin == (INDEX_TYPE)NumericLimits<int64_t>::Minimum()) {
		begin = 0;
	}

	if (end == (INDEX_TYPE)NumericLimits<int64_t>::Maximum()) {
		end = ValueLength<INPUT_TYPE, INDEX_TYPE>(sliced);
	}
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
static void ExecuteConstantSlice(Vector &result, Vector &str_vector, Vector &begin_vector, Vector &end_vector,
                                 optional_ptr<Vector> step_vector, const idx_t count, SelectionVector &sel,
                                 idx_t &sel_idx, optional_ptr<Vector> result_child_vector) {
	auto result_data = ConstantVector::GetData<INPUT_TYPE>(result);
	auto str_data = ConstantVector::GetData<INPUT_TYPE>(str_vector);
	auto begin_data = ConstantVector::GetData<INDEX_TYPE>(begin_vector);
	auto end_data = ConstantVector::GetData<INDEX_TYPE>(end_vector);
	auto step_data = step_vector ? ConstantVector::GetData<INDEX_TYPE>(*step_vector) : nullptr;

	auto str = str_data[0];
	auto begin = begin_data[0];
	auto end = end_data[0];
	auto step = step_data ? step_data[0] : 1;

	CheckBeginAndEnd(begin, end, step, str);

	auto str_valid = !ConstantVector::IsNull(str_vector);
	auto begin_valid = !ConstantVector::IsNull(begin_vector);
	auto end_valid = !ConstantVector::IsNull(end_vector);
	auto step_valid = step_vector && !ConstantVector::IsNull(*step_vector);

	// Clamp offsets
	bool clamp_result = false;
	if (str_valid && begin_valid && end_valid && (step_valid || step == 1)) {
		clamp_result = ClampSlice(str, begin, end);
	}

	auto sel_length = 0;
	if (step_vector && step_valid && str_valid && begin_valid && end_valid && step != 1 && end - begin > 0) {
		sel_length = CalculateSliceLength(begin, end, step, step_valid);
		sel.Initialize(sel_length);
	}

	// Try to slice
	if (!str_valid || !begin_valid || !end_valid || (step_vector && !step_valid) || !clamp_result) {
		ConstantVector::SetNull(result, true);
	} else if (step == 1) {
		result_data[0] = SliceValue<INPUT_TYPE, INDEX_TYPE>(result, str, begin, end);
	} else {
		result_data[0] = SliceValueWithSteps<INPUT_TYPE, INDEX_TYPE>(result, sel, str, begin, end, step, sel_idx);
	}

	if (step_vector && step != 0 && end - begin > 0) {
		result_child_vector->Slice(sel, sel_length);
	}
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
static void ExecuteFlatSlice(Vector &result, Vector &list_vector, Vector &begin_vector, Vector &end_vector,
                             optional_ptr<Vector> step_vector, const idx_t count, SelectionVector &sel, idx_t &sel_idx,
                             optional_ptr<Vector> result_child_vector) {
	UnifiedVectorFormat list_data, begin_data, end_data, step_data;
	idx_t sel_length = 0;

	list_vector.ToUnifiedFormat(count, list_data);
	begin_vector.ToUnifiedFormat(count, begin_data);
	end_vector.ToUnifiedFormat(count, end_data);
	if (step_vector) {
		step_vector->ToUnifiedFormat(count, step_data);
		sel.Initialize(ListVector::GetListSize(list_vector));
	}

	auto result_data = FlatVector::GetData<INPUT_TYPE>(result);
	auto &result_mask = FlatVector::Validity(result);

	for (idx_t i = 0; i < count; ++i) {
		auto list_idx = list_data.sel->get_index(i);
		auto begin_idx = begin_data.sel->get_index(i);
		auto end_idx = end_data.sel->get_index(i);
		auto step_idx = step_vector ? step_data.sel->get_index(i) : 0;

		auto sliced = ((INPUT_TYPE *)list_data.data)[list_idx];
		auto begin = ((INDEX_TYPE *)begin_data.data)[begin_idx];
		auto end = ((INDEX_TYPE *)end_data.data)[end_idx];
		auto step = step_vector ? ((INDEX_TYPE *)step_data.data)[step_idx] : 1;

		CheckBeginAndEnd(begin, end, step, sliced);

		auto list_valid = list_data.validity.RowIsValid(list_idx);
		auto begin_valid = begin_data.validity.RowIsValid(begin_idx);
		auto end_valid = end_data.validity.RowIsValid(end_idx);
		auto step_valid = step_vector && step_data.validity.RowIsValid(step_idx);

		bool clamp_result = false;
		if (list_valid && begin_valid && end_valid && (step_valid || step == 1)) {
			clamp_result = ClampSlice(sliced, begin, end);
		}

		auto length = 0;
		if (step_vector && step_valid && list_valid && begin_valid && end_valid && end - begin > 0) {
			length = CalculateSliceLength(begin, end, step, step_valid);
		}
		sel_length += length;

		if (!list_valid || !begin_valid || !end_valid || (step_vector && !step_valid) || !clamp_result) {
			result_mask.SetInvalid(i);
		} else if (!step_vector) {
			result_data[i] = SliceValue<INPUT_TYPE, INDEX_TYPE>(result, sliced, begin, end);
		} else {
			result_data[i] =
			    SliceValueWithSteps<INPUT_TYPE, INDEX_TYPE>(result, sel, sliced, begin, end, step, sel_idx);
		}
	}
	if (step_vector) {
		SelectionVector new_sel(sel_length);
		for (idx_t i = 0; i < sel_length; ++i) {
			new_sel.set_index(i, sel.get_index(i));
		}
		result_child_vector->Slice(new_sel, sel_length);
	}
}

template <typename INPUT_TYPE, typename INDEX_TYPE>
static void ExecuteSlice(Vector &result, Vector &list_or_str_vector, Vector &begin_vector, Vector &end_vector,
                         optional_ptr<Vector> step_vector, const idx_t count) {
	optional_ptr<Vector> result_child_vector;
	if (step_vector) {
		result_child_vector = &ListVector::GetEntry(result);
	}

	SelectionVector sel;
	idx_t sel_idx = 0;

	if (result.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		ExecuteConstantSlice<INPUT_TYPE, INDEX_TYPE>(result, list_or_str_vector, begin_vector, end_vector, step_vector,
		                                             count, sel, sel_idx, result_child_vector);
	} else {
		ExecuteFlatSlice<INPUT_TYPE, INDEX_TYPE>(result, list_or_str_vector, begin_vector, end_vector, step_vector,
		                                         count, sel, sel_idx, result_child_vector);
	}
	result.Verify(count);
}

static void ArraySliceFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 3 || args.ColumnCount() == 4);
	D_ASSERT(args.data.size() == 3 || args.data.size() == 4);
	auto count = args.size();

	Vector &list_or_str_vector = args.data[0];
	Vector &begin_vector = args.data[1];
	Vector &end_vector = args.data[2];

	optional_ptr<Vector> step_vector;
	if (args.ColumnCount() == 4) {
		step_vector = &args.data[3];
	}

	result.SetVectorType(args.AllConstant() ? VectorType::CONSTANT_VECTOR : VectorType::FLAT_VECTOR);
	switch (result.GetType().id()) {
	case LogicalTypeId::LIST: {
		// Share the value dictionary as we are just going to slice it
		if (list_or_str_vector.GetVectorType() != VectorType::FLAT_VECTOR &&
		    list_or_str_vector.GetVectorType() != VectorType::CONSTANT_VECTOR) {
			list_or_str_vector.Flatten(count);
		}
		ListVector::ReferenceEntry(result, list_or_str_vector);
		ExecuteSlice<list_entry_t, int64_t>(result, list_or_str_vector, begin_vector, end_vector, step_vector, count);
		break;
	}
	case LogicalTypeId::VARCHAR: {
		ExecuteSlice<string_t, int64_t>(result, list_or_str_vector, begin_vector, end_vector, step_vector, count);
		break;
	}
	default:
		throw NotImplementedException("Specifier type not implemented");
	}
}

static unique_ptr<FunctionData> ArraySliceBind(ClientContext &context, ScalarFunction &bound_function,
                                               vector<unique_ptr<Expression>> &arguments) {
	D_ASSERT(arguments.size() == 3 || arguments.size() == 4);
	D_ASSERT(bound_function.arguments.size() == 3 || bound_function.arguments.size() == 4);

	switch (arguments[0]->return_type.id()) {
	case LogicalTypeId::LIST:
		// The result is the same type
		bound_function.return_type = arguments[0]->return_type;
		break;
	case LogicalTypeId::VARCHAR:
		// string slice returns a string
		if (bound_function.arguments.size() == 4) {
			throw NotImplementedException(
			    "Slice with steps has not been implemented for string types, you can consider rewriting your query as "
			    "follows:\n SELECT array_to_string((str_split(string, '')[begin:end:step], '');");
		}
		bound_function.return_type = arguments[0]->return_type;
		for (idx_t i = 1; i < 3; i++) {
			bound_function.arguments[i] = LogicalType::BIGINT;
		}
		break;
	case LogicalTypeId::SQLNULL:
	case LogicalTypeId::UNKNOWN:
		bound_function.arguments[0] = LogicalTypeId::UNKNOWN;
		bound_function.return_type = LogicalType::SQLNULL;
		break;
	default:
		throw BinderException("ARRAY_SLICE can only operate on LISTs and VARCHARs");
	}

	return make_uniq<VariableReturnBindData>(bound_function.return_type);
}

ScalarFunctionSet ListSliceFun::GetFunctions() {
	// the arguments and return types are actually set in the binder function
	ScalarFunction fun({LogicalType::ANY, LogicalType::BIGINT, LogicalType::BIGINT}, LogicalType::ANY,
	                   ArraySliceFunction, ArraySliceBind);
	fun.null_handling = FunctionNullHandling::SPECIAL_HANDLING;

	ScalarFunctionSet set;
	set.AddFunction(fun);
	fun.arguments.push_back(LogicalType::BIGINT);
	set.AddFunction(fun);
	return set;
}

} // namespace duckdb
