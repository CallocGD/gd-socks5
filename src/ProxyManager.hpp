/* Inspired By GManager it allows up to retain our settings for setting up a Proxy */
#include "CCJsonTools.hpp"
#include <pthread/pthread.h>
#include <string>
#include <cocos2d.h>
#include <cocos-ext.h>



// NOTE: Im Using String_view for some of these are they are static most of the time...

class ProxyManager : public JsonManager {

protected:
    cocos2d::CCDictionary* m_download;
    std::string proxy_url;
    bool enabled;
    /* Used to prevent Unwanted Events from happening... */
    pthread_mutex_t mutex;
public:
    ProxyManager(){
        m_download = cocos2d::CCDictionary::create();
        /* TODO : Point it to it's save folder location */
        m_filename = "CCProxyManager";
    }
    void encodeDataTo(CCJsonReader* reader);
    void saveDataTo(CCJsonWriter* writer);
    void firstload();

    static ProxyManager* sharedState();

    void clearProxy(std::string proxy);
    void setProxy(std::string proxy);
    /* Obtains our proxy url this will lock in a mutex and then release it... */
    std::string getProxy();
    bool isEnabled();


    bool isDLActive();
    void setDLToActive(std::string_view tag);
    void removeDLFromActive(std::string_view tag);

    /* Http Stuff on Our End is to make sure our proxy works... */
    void ProcessHttpRequest(std::string_view endpoint, std::string params, std::string_view tag);
    void onProcessHttpRequestCompleted(
        cocos2d::extension::CCHttpClient *client, 
        cocos2d::extension::CCHttpResponse *response
    );

    void handleIt(bool RequestSentSuccessfully, std::string response, std::string tag);

    /* Tests a Proxy To Make sure ours is alive and not only 
    can be connected to but that the firewall doesn't 
    complain about it */
    void TestProxy();

    /* Saves and Releases The ProxyManager */
    ~ProxyManager();
};


