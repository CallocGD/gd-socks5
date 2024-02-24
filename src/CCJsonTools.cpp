#include <stdint.h>
#include <yyjson.h>
#include <string>
#include <cocos2d.h>
#include "CCJsonTools.hpp"

/* Json Manager Is Inspired By GManager but is made to be a modern Version of 
 * GManager because it writes & reads Json files */






static CCJsonWriter* create(){
    CCJsonWriter* result = new CCJsonWriter();
    if (result->init()){
        result->autorelease();
        return result;
    }
    CC_SAFE_DELETE(result);
    return nullptr;
};


/* Returns False When Memory Allocation Fails*/
bool CCJsonWriter::init(){
    doc = yyjson_mut_doc_new(0); 
    if (doc == NULL) 
        return false;
    root = NULL;
    yyjson_mut_doc_set_root(doc, root);
    return true;
};


bool CCJsonWriter::setBool(const char* key, bool obj){
    return yyjson_mut_obj_add_bool(doc, root, key, obj);
}

bool CCJsonWriter::setBool(const char* key, cocos2d::CCBool* obj){
    return yyjson_mut_obj_add_bool(doc, root, key, obj->getValue());
}

bool CCJsonWriter::setDouble(const char* key, double obj){
    return yyjson_mut_obj_add_real(doc, root, key, obj);
}

bool CCJsonWriter::setDouble(const char* key, cocos2d::CCDouble* obj){
    yyjson_mut_obj_add_real(doc, root, key, obj->getValue());
}


bool CCJsonWriter::setInt(const char* key, int64_t obj){
    return yyjson_mut_obj_add_int(doc, root, key, obj);
};

bool CCJsonWriter::setUInt(const char* key, uint64_t obj){
    return yyjson_mut_obj_add_uint(doc, root, key, obj);
};

bool CCJsonWriter::setInt(const char* key, cocos2d::CCInteger* obj){
    return yyjson_mut_obj_add_int(doc, root, key, obj->getValue());
};


bool CCJsonWriter::setStr(const char* key, std::string str){
    return yyjson_mut_obj_add_strn(doc, root, key, str.c_str(), str.length());
}

bool CCJsonWriter::setStr(const char* key, cocos2d::CCString* str){
    return yyjson_mut_obj_add_strn(doc, root, key, str->getCString(), str->length());
}

bool CCJsonWriter::write(const char* fileName, yyjson_write_flag flg, yyjson_write_err* err){
    return yyjson_mut_write_file(fileName, doc, flg, NULL, err);
};

bool CCJsonWriter::write(const char* fileName, yyjson_write_err* err){
    write(fileName, YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE, err);
};


CCJsonWriter::~CCJsonWriter(){
    if (doc != NULL || root != NULL){
        yyjson_mut_doc_free(doc);
    }
}

CCJsonReader* CCJsonReader::create(const char* fileName, yyjson_read_err *err,  yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS){
    CCJsonReader* result = new CCJsonReader();
    if (result->init(fileName, err, flg)){
        result->autorelease();
        return result;
    }
    CC_SAFE_DELETE(result);
    return nullptr;
};


CCJsonReader* CCJsonReader::createWithStr(std::string JsonData, yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS){
    CCJsonReader* result = new CCJsonReader();
    if (result->initWithStr(JsonData, flg)){
        result->autorelease();
        return result;
    }
    CC_SAFE_DELETE(result);
    return nullptr;
}

CCJsonReader* CCJsonReader::createWithCCStr(cocos2d::CCString* JsonData, yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS){
    return createWithStr(JsonData->m_sString, flg);
}


bool CCJsonReader::dumpData(const char* filename){  
    return yyjson_write_file(filename, doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE, NULL, NULL);
}

bool dumpData(std::string_view filename){
    return dumpData(filename.data());
};


yyjson_val* CCJsonReader::get(const char* key){
    return yyjson_obj_get(root, key);
}


bool CCJsonReader::getBool(const char* key){
    return yyjson_get_bool(get(key));
}


const char* CCJsonReader::getCString(const char* key){
    return yyjson_get_str(get(key));
}


std::string CCJsonReader::getString(const char* key){
    auto obj = get(key);
    return std::string(yyjson_get_str(obj), yyjson_get_len(obj));
}



double CCJsonReader::getDouble(const char* key){
    return yyjson_get_real(get(key));
}

int64_t CCJsonReader::getInt(const char* key){
    return yyjson_get_int(get(key));
}

uint64_t CCJsonReader::getUInt(const char* key){
    return yyjson_get_uint(get(key));
}





bool CCJsonReader::initWithStr(std::string JsonData, yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS){
    doc = yyjson_read(JsonData.c_str(), JsonData.length(), flg);
    if (doc == NULL)
        return false;
    root = yyjson_doc_get_root(doc);
    return true;
}


bool CCJsonReader::init(const char* fileName, yyjson_read_err *err,  yyjson_read_flag flg  = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS){
    doc = yyjson_read_file(fileName, flg, NULL, err);
    if (doc == NULL)
        return false;
    root = yyjson_doc_get_root(doc);
    return true;
}


CCJsonReader::~CCJsonReader(){
    if (doc != NULL || root != NULL)
        yyjson_doc_free(doc);
}



/* Json Manager */

bool JsonManager::init(){
    m_setup = true;
    m_saved = false;
    return true;
}

void JsonManager::setup(){
    m_setup = true;
    load();
}


/* Helper functions */
CCJsonReader* JsonManager::open_file(std::string filename){
    return CCJsonReader::create(filename.c_str(), NULL);
}

bool JsonManager::writeToFile(std::string_view filename, CCJsonWriter* writer){
    return writer->write(filename.data(), NULL);
};


bool JsonManager::load(){
    return loadDataFromFile(m_filename);
}

bool JsonManager::loadDataFromFile(std::string filename){
    if (!tryLoadData(filename + ".json")){
        if (!tryLoadData(filename + ".backup.json")){
            firstload();
            return true;
        }
        
        else {
            /* Copy the Save Data from the backup */
            auto reader = open_file(filename + ".backup.json");
            reader->dumpData(filename + ".json");
            reader->release();
        }
        return tryLoadData(filename + ".json");
    }
    return true;
}


void JsonManager::saveGMTo(std::string filename){
    auto writer = CCJsonWriter::create();
    saveDataTo(writer);
    writeToFile(filename, writer);
    writer->release();
}

void JsonManager::save(){
    saveGMTo(m_filename);
}


bool JsonManager::tryLoadData(std::string filename){
    auto reader = open_file(filename);
    /* Non existant check or parsing problems caused by human tampering */
    if (reader == nullptr){
        return false;
    }
    encodeDataTo(reader);
    reader->release();
    return true;
}



