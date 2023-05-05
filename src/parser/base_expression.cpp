#include "duckdb/parser/base_expression.hpp"

#include "duckdb/main/config.hpp"
#include "duckdb/common/printer.hpp"

namespace duckdb {

void BaseExpression::Print() const {
	Printer::Print(ToString());
}

string BaseExpression::GetName() const {
#ifdef DEBUG
	if (DBConfigOptions::debug_print_bindings) {
		return ToString();
	}
#endif
	return !alias.empty() ? alias : ToString();
}

bool BaseExpression::Equals(const BaseExpression *other) const {
	if (!other) {
		return false;
	}
	if (this->expression_class != other->expression_class || this->type != other->type) {
		return false;
	}
	return true;
}

void BaseExpression::Verify() const {
}

} // namespace duckdb
