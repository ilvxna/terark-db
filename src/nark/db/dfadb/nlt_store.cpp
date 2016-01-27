#include "nlt_store.hpp"
#include <nark/int_vector.hpp>
#include <nark/fast_zip_data_store.hpp>
#include <typeinfo>

namespace nark { namespace db { namespace dfadb {

NARK_DB_REGISTER_STORE("nlt", NestLoudsTrieStore);

NestLoudsTrieStore::NestLoudsTrieStore() {
}
NestLoudsTrieStore::~NestLoudsTrieStore() {
}

llong NestLoudsTrieStore::dataStorageSize() const {
	return m_store->mem_size();
}

llong NestLoudsTrieStore::numDataRows() const {
	return m_store->num_records();
}

void NestLoudsTrieStore::getValueAppend(llong id, valvec<byte>* val, DbContext* ctx) const {
	m_store->get_record_append(size_t(id), val);
}

StoreIterator* NestLoudsTrieStore::createStoreIterForward(DbContext*) const {
	return nullptr; // not needed
}

StoreIterator* NestLoudsTrieStore::createStoreIterBackward(DbContext*) const {
	return nullptr; // not needed
}

template<class Class>
static
Class* doBuild(const NestLoudsTrieConfig& conf,
			   const Schema& schema, SortableStrVec& strVec) {
	std::unique_ptr<Class> trie(new Class());
	trie->build_from(strVec, conf);
	return trie.release();
}

static
void initConfigFromSchema(NestLoudsTrieConfig& conf, const Schema& schema) {
	conf.initFromEnv();
	if (schema.m_sufarrMinFreq) {
		conf.saFragMinFreq = (byte_t)schema.m_sufarrMinFreq;
	}
	if (schema.m_minFragLen) {
		conf.minFragLen = schema.m_minFragLen;
	}
	if (schema.m_maxFragLen) {
		conf.maxFragLen = schema.m_maxFragLen;
	}
	if (schema.m_nltDelims.size()) {
		conf.setBestDelims(schema.m_nltDelims.c_str());
	}
}

static
DataStore* nltBuild(const Schema& schema, SortableStrVec& strVec) {
	NestLoudsTrieConfig conf;
	initConfigFromSchema(conf, schema);
	switch (schema.m_rankSelectClass) {
	case -256:
		return doBuild<NestLoudsTrieDataStore_IL>(conf, schema, strVec);
	case +256:
		return doBuild<NestLoudsTrieDataStore_SE>(conf, schema, strVec);
	case +512:
		return doBuild<NestLoudsTrieDataStore_SE_512>(conf, schema, strVec);
	default:
		fprintf(stderr, "WARN: invalid schema(%s).rs = %d, use default: se_512\n"
					  , schema.m_name.c_str(), schema.m_rankSelectClass);
		return doBuild<NestLoudsTrieDataStore_SE_512>(conf, schema, strVec);
	}
}

void NestLoudsTrieStore::build(const Schema& schema, SortableStrVec& strVec) {
	if (schema.m_useFastZip) {
		std::unique_ptr<FastZipDataStore> fzds(new FastZipDataStore());
		NestLoudsTrieConfig conf;
		initConfigFromSchema(conf, schema);
		fzds->build_from(strVec, conf);
		m_store.reset(fzds.release());
	}
	else {
		m_store.reset(nltBuild(schema, strVec));
	}
}

void NestLoudsTrieStore::load(PathRef path) {
	std::string fpath = fstring(path.string()).endsWith(".nlt")
					  ? path.string()
					  : path.string() + ".nlt";
	m_store.reset(DataStore::load_from(fpath));
}

void NestLoudsTrieStore::save(PathRef path) const {
	std::string fpath = fstring(path.string()).endsWith(".nlt")
						? path.string()
						: path.string() + ".nlt";
	if (BaseDFA* dfa = dynamic_cast<BaseDFA*>(&*m_store)) {
		dfa->save_mmap(fpath.c_str());
	}
	else {
		auto fzds = dynamic_cast<FastZipDataStore*>(m_store.get());
		fzds->save_mmap(fpath);
	}
}

}}} // namespace nark::db::dfadb
