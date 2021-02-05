#include "duckdb/function/pragma/pragma_functions.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"

namespace duckdb {

string PragmaTableInfo(ClientContext &context, FunctionParameters parameters) {
	return StringUtil::Format("SELECT * FROM pragma_table_info('%s')", parameters.values[0].ToString());
}

string PragmaShowTables(ClientContext &context, FunctionParameters parameters) {
	return "SELECT name FROM sqlite_master() ORDER BY name";
}

string PragmaDatabaseList(ClientContext &context, FunctionParameters parameters) {
	return "SELECT * FROM pragma_database_list() ORDER BY 1";
}

string PragmaCollations(ClientContext &context, FunctionParameters parameters) {
	return "SELECT * FROM pragma_collations() ORDER BY 1";
}

string PragmaFunctions(ClientContext &context, FunctionParameters parameters) {
	return "SELECT * FROM pragma_functions() ORDER BY 1";
}

string PragmaShow(ClientContext &context, FunctionParameters parameters) {
	// PRAGMA table_info but with some aliases
	return StringUtil::Format(
	    "SELECT name AS \"Field\", type as \"Type\", CASE WHEN \"notnull\" THEN 'NO' ELSE 'YES' END AS \"Null\", "
	    "NULL AS \"Key\", dflt_value AS \"Default\", NULL AS \"Extra\" FROM pragma_table_info('%s')",
	    parameters.values[0].ToString());
}

string PragmaVersion(ClientContext &context, FunctionParameters parameters) {
	return "SELECT * FROM pragma_version()";
}

string PragmaImportDatabase(ClientContext &context, FunctionParameters parameters) {
	auto &fs = FileSystem::GetFileSystem(context);
	string query;
	// read the "shema.sql" and "load.sql" files
	vector<string> files = {"schema.sql", "load.sql"};
	for (auto &file : files) {
		auto file_path = fs.JoinPath(parameters.values[0].ToString(), file);
		auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
		auto fsize = fs.GetFileSize(*handle);
		auto buffer = unique_ptr<char[]>(new char[fsize]);
		fs.Read(*handle, buffer.get(), fsize);

		query += string(buffer.get(), fsize);
	}
	return query;
}

string PragmaDatabaseSize(ClientContext &context, FunctionParameters parameters) {
	return "SELECT * FROM pragma_database_size()";
}

void PragmaQueries::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(PragmaFunction::PragmaCall("table_info", PragmaTableInfo, {LogicalType::VARCHAR}));
	set.AddFunction(PragmaFunction::PragmaStatement("show_tables", PragmaShowTables));
	set.AddFunction(PragmaFunction::PragmaStatement("database_list", PragmaDatabaseList));
	set.AddFunction(PragmaFunction::PragmaStatement("collations", PragmaCollations));
	set.AddFunction(PragmaFunction::PragmaCall("show", PragmaShow, {LogicalType::VARCHAR}));
	set.AddFunction(PragmaFunction::PragmaStatement("version", PragmaVersion));
	set.AddFunction(PragmaFunction::PragmaStatement("database_size", PragmaDatabaseSize));
	set.AddFunction(PragmaFunction::PragmaStatement("functions", PragmaFunctions));
	set.AddFunction(PragmaFunction::PragmaCall("import_database", PragmaImportDatabase, {LogicalType::VARCHAR}));
}

} // namespace duckdb
