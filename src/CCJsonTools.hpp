/* Json Manager Is Inspired By GManager but is made to be a modern Version of 
 * GManager because it writes & reads Json files *
 *
 *
 * You Have my permission to use These tools for other or Rip this code for other 
 * purposes, It's under an MIT License after all... - Calloc */
#include <stdint.h>
#include <yyjson.h>
#include <string>
#include <cocos2d.h>

/* Used For Writing and Saving Json Data with yyjson under the hood 
 * which has faster speeds than rapidjson 
 *
 * Note that it can only handle linear information 
 * I haven't implemented arrarys or outside Objects yet - Calloc */
class CCJsonWriter : public cocos2d::CCObject {
public:
    yyjson_mut_doc *doc;
    yyjson_mut_val *root;
    
    CCJsonWriter();
    static CCJsonWriter* create();
    /* Returns False When Memory Allocation Fails*/
    bool init();

    bool setBool(const char* key, bool obj);
    bool setBool(const char* key, cocos2d::CCBool* obj);
    bool setDouble(const char* key, double obj);
    bool setDouble(const char* key, cocos2d::CCDouble* obj);
    bool setStr(const char* key, std::string str);
    bool setStr(const char* key, cocos2d::CCString* str);
    bool setInt(const char* key, int64_t obj);
    bool setUInt(const char* key, uint64_t obj);
    bool setInt(const char* key, cocos2d::CCInteger* obj);

    /* Writes Json data to a file for safe-keeping */
    bool write(const char* fileName, yyjson_write_flag flg, yyjson_write_err* err);

    /* Writes Json data to a file for safe-keeping Note that this function will set the yyjson flags
     * to YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE  if this is not what you want use the 
     * other overload function */
    bool write(const char* fileName, yyjson_write_err* err);


    /* TODO */ 
    /* std::string writeStr(yyjson_write_flag flg) */



    ~CCJsonWriter();

};


    

/* Used To Read Json Files Note that it can only handle linear information 
 *
 * Note that it can only handle linear information 
 * I haven't implemented arrarys or outside Objects jet - Calloc */
class CCJsonReader : public cocos2d::CCObject {
public:
    yyjson_doc* doc;
    yyjson_val* root;

    CCJsonReader();
    /* Creates and Opens a file for reading returns NULL if it fails... */
    static CCJsonReader* create(const char* fileName, yyjson_read_err *err, yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS);
    static CCJsonReader* createWithStr(std::string JsonData,  yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS);
    static CCJsonReader* createWithCCStr(cocos2d::CCString* JsonData, yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS);

    /* Used to dump the data made or make a complete copy of it Returns False if data failed to back up... */
    bool dumpData(const char* filename);
    bool dumpData(std::string_view filename);
    /* Used as a middle man for getting yyjson values... */
    yyjson_val* get(const char* key);


    bool getBool(const char* key);
    
    /* Returns a C++ string */
    std::string getString(const char* key);

    /* Gets a C String */
    const char* getCString(const char* key);


    double getDouble(const char* key);
    int64_t getInt(const char* key);
    uint64_t getUInt(const char* key);

    /* Returns False When Memory Allocation Fails or file wasn't found... */
    bool init(const char* fileName, yyjson_read_err *err, yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS);
    bool initWithStr(std::string JsonData,  yyjson_read_flag flg  = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS);

    ~CCJsonReader();
};





/* Inspired by Robtop's GManager Class Object */
class JsonManager : cocos2d::CCNode {
public:

    std::string m_filename;
    bool m_setup;
    bool m_saved;

    bool init();
    void setup();

    /* Helper functions */
    
    CCJsonReader* open_file(std::string filename);
    bool writeToFile(std::string_view filename, CCJsonWriter* writer);

    /* Main Functions */

    bool load();
    bool loadDataFromFile(std::string filename);
    void save();
    void saveGMTo(std::string filename);
    /* Attempts to load the save-data from the files returns false if the file doesn't exist or has been tampered with */
    bool tryLoadData(std::string filename);

    /* virtuals */
    virtual void firstload();
    /* encoding data means to send the CCJsonReader's data to the manager's class members*/
    virtual void encodeDataTo(CCJsonReader* writer);
    /* Save Data to is a deloading function when a Manager is deallocating */
    virtual void saveDataTo(CCJsonWriter* writer);

};
