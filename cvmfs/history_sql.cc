/**
 * This file is part of the CernVM File System.
 */

#include "history_sql.h"


namespace history {

const float HistoryDatabase::kLatestSchema = 1.0;
const float HistoryDatabase::kLatestSupportedSchema = 1.0;
const unsigned HistoryDatabase::kLatestSchemaRevision = 1;

const std::string HistoryDatabase::kFqrnKey = "fqrn";


/**
 * This method creates a new database file and initializes the database schema.
 */
bool HistoryDatabase::CreateEmptyDatabase() {
  assert (read_write());
  return sqlite::Sql(sqlite_db(),
    "CREATE TABLE tags (name TEXT, hash TEXT, revision INTEGER, "
    "  timestamp INTEGER, channel INTEGER, description TEXT, size INTEGER, "
    "  CONSTRAINT pk_tags PRIMARY KEY (name))").Execute();
}


bool HistoryDatabase::InsertInitialValues(const std::string &repository_name) {
  assert (read_write());
  return this->SetProperty(kFqrnKey, repository_name);
}


bool HistoryDatabase::CheckSchemaCompatibility() {
  return ! ((schema_version() < kLatestSupportedSchema - kSchemaEpsilon) ||
            (schema_version() > kLatestSchema          + kSchemaEpsilon));
}


bool HistoryDatabase::LiveSchemaUpgradeIfNecessary() {
  assert (read_write());

  if (schema_revision() == 0) {
    LogCvmfs(kLogHistory, kLogDebug, "upgrading schema revision");

    sqlite::Sql sql_upgrade(sqlite_db(), "ALTER TABLE tags ADD size INTEGER;");
    if (!sql_upgrade.Execute()) {
      LogCvmfs(kLogHistory, kLogDebug, "failed to upgrade tags table");
      return false;
    }

    set_schema_revision(1);
    if (! StoreSchemaRevision()) {
      LogCvmfs(kLogHistory, kLogDebug, "failed tp upgrade schema revision");
      return false;
    }
  }

  return true;
}


//------------------------------------------------------------------------------


const std::string SqlRetrieveTag::tag_database_fields =
  "name, hash, revision, timestamp, channel, description, size";

const std::string& SqlRetrieveTag::GetDatabaseFields() const {
  return SqlRetrieveTag::tag_database_fields;
}

History::Tag SqlRetrieveTag::RetrieveTag() const {
  History::Tag result;
  result.name        = RetrieveString(0);
  result.root_hash   = shash::MkFromHexPtr(shash::HexPtr(RetrieveString(1)));
  result.revision    = RetrieveInt64(2);
  result.timestamp   = RetrieveInt64(3);
  result.channel     = static_cast<History::UpdateChannel>(RetrieveInt64(4));
  result.description = RetrieveString(5);
  result.size        = RetrieveInt64(6);
  return result;
}


//------------------------------------------------------------------------------


SqlInsertTag::SqlInsertTag(const HistoryDatabase *database) {
  const std::string stmt =
    "INSERT INTO tags (" + GetDatabaseFields() + ")"
    "VALUES (" + GetDatabasePlaceholders() + ");";
  const bool success = Init(database->sqlite_db(), stmt);
  assert (success);
}

const std::string SqlInsertTag::tag_database_placeholders =
  ":name, :hash, :revision, :timestamp, :channel, :description, :size";

const std::string& SqlInsertTag::GetDatabasePlaceholders() const {
  return SqlInsertTag::tag_database_placeholders;
}

bool SqlInsertTag::BindTag(const History::Tag &tag) {
  return (
    BindText         (1, tag.name)                 &&
    BindTextTransient(2, tag.root_hash.ToString()) && // temporary from ToString
    BindInt64        (3, tag.revision)             &&
    BindInt64        (4, tag.timestamp)            &&
    BindInt64        (5, tag.channel)              &&
    BindText         (6, tag.description)          &&
    BindInt64        (7, tag.size)
  );
}


SqlRemoveTag::SqlRemoveTag(const HistoryDatabase *database) {
  const std::string stmt = "DELETE FROM tags WHERE name = :name;";
  const bool success = Init(database->sqlite_db(), stmt);
  assert (success);
}

bool SqlRemoveTag::BindName(const std::string &name) {
  return BindText(1, name);
}


SqlFindTag::SqlFindTag(const HistoryDatabase *database) {
  const std::string stmt =
    "SELECT " + GetDatabaseFields() + " FROM tags "
    "WHERE name = :name LIMIT 1;";
  const bool success = Init(database->sqlite_db(), stmt);
  assert (success);
}

bool SqlFindTag::BindName(const std::string &name) {
  return BindText(1, name);
}


SqlCountTags::SqlCountTags(const HistoryDatabase *database) {
  const bool success = Init(database->sqlite_db(),
                            "SELECT count(*) FROM tags;");
  assert (success);
}

int SqlCountTags::RetrieveCount() const {
  return RetrieveInt64(0);
}


SqlListTags::SqlListTags(const HistoryDatabase *database) {
  const bool success = Init(database->sqlite_db(),
                            "SELECT " + GetDatabaseFields() + " FROM tags "
                            "ORDER BY revision DESC;");
  assert (success);
}

SqlGetChannelTips::SqlGetChannelTips(const HistoryDatabase *database) {
  const bool success = Init(database->sqlite_db(),
                            "SELECT " + GetDatabaseFields() + ", "
                            "  MAX(revision) AS max_rev "
                            "FROM tags "
                            "GROUP BY channel;");
  assert (success);
}

SqlGetHashes::SqlGetHashes(const HistoryDatabase *database) {
  const bool success = Init(database->sqlite_db(),
                            "SELECT DISTINCT hash FROM tags "
                            "ORDER BY revision ASC");
  assert (success);
}

shash::Any SqlGetHashes::RetrieveHash() const {
  return shash::MkFromHexPtr(shash::HexPtr(RetrieveString(0)));
}







bool SqlTag::BindTag(const History::Tag &tag) {
  return (
    BindText(1, tag.name) &&
    BindTextTransient(2, tag.root_hash.ToString()) && // temporary from ToString
    BindInt64(3, tag.revision) &&
    BindInt64(4, tag.timestamp) &&
    BindInt64(5, tag.channel) &&
    BindText(6, tag.description) &&
    BindInt64(7, tag.size)
  );
}


History::Tag SqlTag::RetrieveTag() {
  History::Tag result;
  result.name = std::string(reinterpret_cast<const char *>(RetrieveText(0)));
  const std::string hash_str(reinterpret_cast<const char *>(RetrieveText(1)));
  result.root_hash = shash::MkFromHexPtr(shash::HexPtr(hash_str));
  result.revision = RetrieveInt64(2);
  result.timestamp = RetrieveInt64(3);
  result.channel = static_cast<History::UpdateChannel>(RetrieveInt64(4));
  result.description = std::string(reinterpret_cast<const char *>(RetrieveText(5)));
  result.size = RetrieveInt64(6);
  return result;
}








}; /* namespace history */
