/* Used as an Overridable Solution for CCHttpClient Since we can't 
 * hack into CurlRaii Due to Limitations caused by Broma , 
 * So this was my Workaround Idea Will simply just make a subclass for 
 * CCHttpClient in order to mimic as much of it's behaviors as we can to make 
 * our Requests to robtop's server look as innocent and legitimate as possible...
 */


#include <cocos2d.h>
#include <cocos-ext.h>


/* 
 * Used to Override CCHttpRequest's Behaviors while Keeping Default Functions In tact
 * so That Proxies can be supported And disattached ... 
 */
namespace ProxyOverride {

/* You What they say if it aint broken dont fix... */


/* This is mainly provided to be just an Override Class without having to manipulate CCHttpClient */
class CCProxyClient: public cocos2d::CCObject {
public:
    /** Return the shared instance **/
    static CCProxyClient *getInstance();
    
    /** Relase the shared instance **/
    static void destroyInstance();
        
    /**
     * Add a get request to task queue
     * @param request a CCHttpRequest object, which includes url, response callback etc.
                      please make sure request->_requestData is clear before calling "send" here.
     * @return NULL
     */
    void send(cocos2d::extension::CCHttpRequest* request);
  
    
    /**
     * Change the connect timeout
     * @param timeout 
     * @return NULL
     */
    inline void setTimeoutForConnect(int value) {_timeoutForConnect = value;};
    
    /**
     * Get connect timeout
     * @return int
     *
     */
    inline int getTimeoutForConnect() {return _timeoutForConnect;}
    
    
    /**
     * Change the download timeout
     * @param value
     * @return NULL
     */
    inline void setTimeoutForRead(int value) {_timeoutForRead = value;};
    

    /**
     * Get download timeout
     * @return int
     */
    inline int getTimeoutForRead() {return _timeoutForRead;};
        
private:
    CCProxyClient();
    // virtual ~CCProxyClient();
    bool init(void);
    
    /**
     * Init pthread mutex, semaphore, and create new thread for http requests
     * @return bool
     */
    bool lazyInitThreadSemphore();
    /** Poll function called from main thread to dispatch callbacks when http requests finished **/
    void dispatchResponseCallbacks(float delta);
    
private:
    int _timeoutForConnect;
    int _timeoutForRead;
    
    /* TODO: FIX PROPERTIES */
    // properties
    // HttpRequestType             _requestType;    /// kHttpRequestGet, kHttpRequestPost or other enums
    gd::string                 _url;            /// target url that this request is sent to
    gd::vector<char>           _requestData;    /// used for POST
    gd::string                 _tag;            /// user defined tag, to identify different requests in response callback
    CCObject*          _pTarget;        /// callback target of pSelector function
    // SEL_HttpResponse            _pSelector;      /// callback function, e.g. MyLayer::onHttpResponse(CCHttpClient *sender, CCHttpResponse * response)
    void*                       _pUserData;      /// You can add your customed data here 
    gd::vector<gd::string>    _headers;		      /// custom http headers

    RT_ADD(
        int _requestTypeGJ;
        bool _shouldCancel;
        int _downloadProgress;
        int _readTimeout;
    )
};


}






