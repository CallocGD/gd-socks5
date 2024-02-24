/****************************************************************************
 Copyright (c) 2010-2012 cocos2d-x.org
 Copyright (c) 2012 greathqy
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/




#include "HttpClient.h"
#include "CCProxyClient.hpp"
// #include "platform/CCThread.h"

#include "ProxyManager.hpp"


#if (CC_TARGET_PLATFORM != CC_PLATFORM_WINRT) && (CC_TARGET_PLATFORM != CC_PLATFORM_WP8)

#include <queue>
#include <pthread.h>
#include <errno.h>

#include "curl/curl.h"


namespace ProxyOverride { 


static pthread_t        s_networkThread;
static pthread_mutex_t  s_requestQueueMutex;
static pthread_mutex_t  s_responseQueueMutex;

static pthread_mutex_t		s_SleepMutex;
static pthread_cond_t		s_SleepCondition;

static unsigned long    s_asyncRequestCount = 0;

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
typedef int int32_t;
#endif

static bool need_quit = false;

static cocos2d::CCArray* s_requestQueue = NULL;
static cocos2d::CCArray* s_responseQueue = NULL;

static CCProxyClient *s_pHttpClient = NULL; // pointer to singleton

static char s_errorBuffer[CURL_ERROR_SIZE];

typedef size_t (*write_callback)(void *ptr, size_t size, size_t nmemb, void *stream);

// Callback function used by libcurl for collect response data
static size_t writeData(void *ptr, size_t size, size_t nmemb, void *stream)
{
    std::vector<char> *recvBuffer = (std::vector<char>*)stream;
    size_t sizes = size * nmemb;
    
    // add data to the end of recvBuffer
    // write data maybe called more than once in a single request
    recvBuffer->insert(recvBuffer->end(), (char*)ptr, (char*)ptr+sizes);
    
    return sizes;
}

// Callback function used by libcurl for collect header data
static size_t writeHeaderData(void *ptr, size_t size, size_t nmemb, void *stream)
{
    std::vector<char> *recvBuffer = (std::vector<char>*)stream;
    size_t sizes = size * nmemb;
    
    // add data to the end of recvBuffer
    // write data maybe called more than once in a single request
    recvBuffer->insert(recvBuffer->end(), (char*)ptr, (char*)ptr+sizes);
    
    return sizes;
}


static int processGetTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *errorCode, write_callback headerCallback, void *headerStream);
static int processPostTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *errorCode, write_callback headerCallback, void *headerStream);
static int processPutTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *errorCode, write_callback headerCallback, void *headerStream);
static int processDeleteTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *errorCode, write_callback headerCallback, void *headerStream);
// int processDownloadTask(HttpRequest *task, write_callback callback, void *stream, int32_t *errorCode);


// Worker thread
static void* networkThread(void *data)
{    
    cocos2d::extension::CCHttpRequest *request = NULL;
    
    while (true) 
    {
        if (need_quit)
        {
            break;
        }
        
        // step 1: send http request if the requestQueue isn't empty
        request = NULL;
        
        pthread_mutex_lock(&s_requestQueueMutex); //Get request task from queue
        if (0 != s_requestQueue->count())
        {
            request = dynamic_cast<cocos2d::extension::CCHttpRequest*>(s_requestQueue->objectAtIndex(0));
            s_requestQueue->removeObjectAtIndex(0);  
            // request's refcount = 1 here
        }
        pthread_mutex_unlock(&s_requestQueueMutex);
        
        if (NULL == request)
        {
        	// Wait for http request tasks from main thread
        	pthread_cond_wait(&s_SleepCondition, &s_SleepMutex);
            continue;
        }
        
        // step 2: libcurl sync access
        
        // Create a HttpResponse object, the default setting is http access failed
        cocos2d::extension::CCHttpResponse *response = new cocos2d::extension::CCHttpResponse(request);
        
        // request's refcount = 2 here, it's retained by HttpRespose constructor
        request->release();
        // ok, refcount = 1 now, only HttpResponse hold it.
        
        int32_t responseCode = -1;
        int retValue = 0;

        // Process the request -> get response packet
        switch (request->getRequestType())
        {
            case cocos2d::extension::CCHttpRequest::kHttpGet: // HTTP GET
                retValue = processGetTask(request,
                                          writeData, 
                                          response->getResponseData(), 
                                          &responseCode,
                                          writeHeaderData,
                                          response->getResponseHeader());
                break;
            
            case cocos2d::extension::CCHttpRequest::kHttpPost: // HTTP POST
                retValue = processPostTask(request,
                                           writeData, 
                                           response->getResponseData(), 
                                           &responseCode,
                                           writeHeaderData,
                                           response->getResponseHeader());
                break;

            case cocos2d::extension::CCHttpRequest::kHttpPut:
                retValue = processPutTask(request,
                                          writeData,
                                          response->getResponseData(),
                                          &responseCode,
                                          writeHeaderData,
                                          response->getResponseHeader());
                break;

            case cocos2d::extension::CCHttpRequest::kHttpDelete:
                retValue = processDeleteTask(request,
                                             writeData,
                                             response->getResponseData(),
                                             &responseCode,
                                             writeHeaderData,
                                             response->getResponseHeader());
                break;
            
            default:
                CCAssert(true, "CCProxyClient: unkown request type, only GET and POSt are supported");
                break;
        }
                
        // write data to HttpResponse
        response->setResponseCode(responseCode);
        
        if (retValue != 0) 
        {
            response->setSucceed(false);
            response->setErrorBuffer(s_errorBuffer);
        }
        else
        {
            response->setSucceed(true);
        }

        
        // add response packet into queue
        pthread_mutex_lock(&s_responseQueueMutex);
        s_responseQueue->addObject(response);
        pthread_mutex_unlock(&s_responseQueueMutex);
        
        // resume dispatcher selector
        cocos2d::CCDirector::sharedDirector()->getScheduler()->resumeTarget(CCProxyClient::getInstance());
    }
    
    // cleanup: if worker thread received quit signal, clean up un-completed request queue
    pthread_mutex_lock(&s_requestQueueMutex);
    s_requestQueue->removeAllObjects();
    pthread_mutex_unlock(&s_requestQueueMutex);
    s_asyncRequestCount -= s_requestQueue->count();
    
    if (s_requestQueue != NULL) {
        
        pthread_mutex_destroy(&s_requestQueueMutex);
        pthread_mutex_destroy(&s_responseQueueMutex);
        
        pthread_mutex_destroy(&s_SleepMutex);
        pthread_cond_destroy(&s_SleepCondition);

        s_requestQueue->release();
        s_requestQueue = NULL;
        s_responseQueue->release();
        s_responseQueue = NULL;
    }

    pthread_exit(NULL);
    
    return 0;
}

//Configure curl's timeout property
static bool configureCURL(CURL *handle)
{
    if (!handle) {
        return false;
    }
    
    int32_t code;
    code = curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, s_errorBuffer);
    if (code != CURLE_OK) {
        return false;
    }
    code = curl_easy_setopt(handle, CURLOPT_TIMEOUT, CCProxyClient::getInstance()->getTimeoutForRead());
    if (code != CURLE_OK) {
        return false;
    }
    code = curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, CCProxyClient::getInstance()->getTimeoutForConnect());
    if (code != CURLE_OK) {
        return false;
    }
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

    // FIXED #3224: The subthread of CCProxyClient interrupts main thread if timeout comes.
    // Document is here: http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTNOSIGNAL 
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

    return true;
}

class CURLRaii
{
    /// Instance of CURL
    CURL *m_curl;
    /// Keeps custom header data
    curl_slist *m_headers;
public:
    CURLRaii()
        : m_curl(curl_easy_init())
        , m_headers(NULL)
    {
    }

    ~CURLRaii()
    {
        if (m_curl)
            curl_easy_cleanup(m_curl);
        /* free the linked list for header data */
        if (m_headers)
            curl_slist_free_all(m_headers);
    }

    template <class T>
    bool setOption(CURLoption option, T data)
    {
        return CURLE_OK == curl_easy_setopt(m_curl, option, data);
    }

    /**
     * @brief Inits CURL instance for common usage
     * @param request Null not allowed
     * @param callback Response write callback
     * @param stream Response write stream
     */
    bool init(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, write_callback headerCallback, void *headerStream)
    {
        if (!m_curl)
            return false;
        if (!configureCURL(m_curl))
            return false;

        /* get custom header data (if set) */
       	gd::vector<gd::string> headers=request->getHeaders();
        if(!headers.empty())
        {
            /* append custom headers one by one */
            for (gd::vector<gd::string>::iterator it = headers.begin(); it != headers.end(); ++it)
                m_headers = curl_slist_append(m_headers,it->c_str());
            /* set custom headers for curl */
            if (!setOption(CURLOPT_HTTPHEADER, m_headers))
                return false;
        }
        /* Our Injection Begins Here so that we don't have to do anymore than this one...  */
        /* inner mutextes will lock and then unlock in our Manager as a safety measure... */
        auto PM = ProxyManager::sharedState();
        if (PM->isEnabled()){
            if (setOption(CURLOPT_PROXY, PM->getProxy().c_str()))
                return false;
        }
        
        return setOption(CURLOPT_URL, request->getUrl())
                && setOption(CURLOPT_WRITEFUNCTION, callback)
                && setOption(CURLOPT_WRITEDATA, stream)
                && setOption(CURLOPT_HEADERFUNCTION, headerCallback)
                && setOption(CURLOPT_HEADERDATA, headerStream);
    }

    /// @param responseCode Null not allowed
    bool perform(int *responseCode)
    {
        if (CURLE_OK != curl_easy_perform(m_curl))
            return false;
        CURLcode code = curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, responseCode);
        if (code != CURLE_OK || *responseCode != 200)
            return false;
        
        // Get some mor data.
        
        return true;
    }
};

//Process Get Request
static int processGetTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *responseCode, write_callback headerCallback, void *headerStream)
{
    CURLRaii curl;
    bool ok = curl.init(request, callback, stream, headerCallback, headerStream)
            && curl.setOption(CURLOPT_FOLLOWLOCATION, true)
            && curl.perform(responseCode);
    return ok ? 0 : 1;
}

//Process POST Request
static int processPostTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *responseCode, write_callback headerCallback, void *headerStream)
{
    CURLRaii curl;
    bool ok = curl.init(request, callback, stream, headerCallback, headerStream)
            /* From Ghidra , Which Told Me Robtop Uses a Special Cookie For The Http Requests - Calloc */
            && curl.setOption(CURLOPT_COOKIE, "gd=1;")
            && curl.setOption(CURLOPT_POST, 1)
            && curl.setOption(CURLOPT_POSTFIELDS, request->getRequestData())
            && curl.setOption(CURLOPT_POSTFIELDSIZE, request->getRequestDataSize())
            && curl.perform(responseCode);
    return ok ? 0 : 1;
}

//Process PUT Request
static int processPutTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *responseCode, write_callback headerCallback, void *headerStream)
{
    CURLRaii curl;
    bool ok = curl.init(request, callback, stream, headerCallback, headerStream)
            && curl.setOption(CURLOPT_CUSTOMREQUEST, "PUT")
            && curl.setOption(CURLOPT_POSTFIELDS, request->getRequestData())
            && curl.setOption(CURLOPT_POSTFIELDSIZE, request->getRequestDataSize())
            && curl.perform(responseCode);
    return ok ? 0 : 1;
}

//Process DELETE Request
static int processDeleteTask(cocos2d::extension::CCHttpRequest *request, write_callback callback, void *stream, int32_t *responseCode, write_callback headerCallback, void *headerStream)
{
    CURLRaii curl;
    bool ok = curl.init(request, callback, stream, headerCallback, headerStream)
            && curl.setOption(CURLOPT_CUSTOMREQUEST, "DELETE")
            && curl.setOption(CURLOPT_FOLLOWLOCATION, true)
            && curl.perform(responseCode);
    return ok ? 0 : 1;
}

// HttpClient implementation
CCProxyClient* CCProxyClient::getInstance()
{
    if (s_pHttpClient == NULL) {
        s_pHttpClient = new CCProxyClient();
    }
    
    return s_pHttpClient;
}

void CCProxyClient::destroyInstance()
{
    CCAssert(s_pHttpClient, "");
    cocos2d::CCDirector::sharedDirector()->getScheduler()->unscheduleSelector(schedule_selector(CCProxyClient::dispatchResponseCallbacks), s_pHttpClient);
    s_pHttpClient->release();
}

CCProxyClient::CCProxyClient()
: _timeoutForConnect(30)
, _timeoutForRead(60)
{
    cocos2d::CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
                    schedule_selector(CCProxyClient::dispatchResponseCallbacks), this, 0, false);
    cocos2d::CCDirector::sharedDirector()->getScheduler()->pauseTarget(this);
}

CCProxyClient::~CCProxyClient()
{
    need_quit = true;
    
    if (s_requestQueue != NULL) {
    	pthread_cond_signal(&s_SleepCondition);
    }
    
    s_pHttpClient = NULL;
}

//Lazy create semaphore & mutex & thread
bool CCProxyClient::lazyInitThreadSemphore()
{
    if (s_requestQueue != NULL) {
        return true;
    } else {
        
        s_requestQueue = new cocos2d::CCArray();
        s_requestQueue->init();
        
        s_responseQueue = new cocos2d::CCArray();
        s_responseQueue->init();
        
        pthread_mutex_init(&s_requestQueueMutex, NULL);
        pthread_mutex_init(&s_responseQueueMutex, NULL);
        
        pthread_mutex_init(&s_SleepMutex, NULL);
        pthread_cond_init(&s_SleepCondition, NULL);

        pthread_create(&s_networkThread, NULL, networkThread, NULL);
        pthread_detach(s_networkThread);
        
        need_quit = false;
    }
    
    return true;
}

//Add a get task to queue
void CCProxyClient::send(cocos2d::extension::CCHttpRequest* request)
{    
    if (false == lazyInitThreadSemphore()) 
    {
        return;
    }
    
    if (!request)
    {
        return;
    }
        
    ++s_asyncRequestCount;
    
    request->retain();
        
    pthread_mutex_lock(&s_requestQueueMutex);
    s_requestQueue->addObject(request);
    pthread_mutex_unlock(&s_requestQueueMutex);
    
    // Notify thread start to work
    pthread_cond_signal(&s_SleepCondition);
}

// Poll and notify main thread if responses exists in queue
void CCProxyClient::dispatchResponseCallbacks(float delta)
{
    // CCLog("CCProxyClient::dispatchResponseCallbacks is running");
    
    cocos2d::extension::CCHttpResponse* response = NULL;
    
    pthread_mutex_lock(&s_responseQueueMutex);
    if (s_responseQueue->count())
    {
        response = dynamic_cast<cocos2d::extension::CCHttpResponse*>(s_responseQueue->objectAtIndex(0));
        s_responseQueue->removeObjectAtIndex(0);
    }
    pthread_mutex_unlock(&s_responseQueueMutex);
    
    if (response)
    {
        --s_asyncRequestCount;
        
        cocos2d::extension::CCHttpRequest *request = response->getHttpRequest();
        CCObject *pTarget = request->getTarget();
        cocos2d::extension::SEL_HttpResponse pSelector = request->getSelector();

        if (pTarget && pSelector) 
        {
            /* They are literally almost the exact same class so therefore this bypass should work */
            (pTarget->*pSelector)(reinterpret_cast<cocos2d::extension::CCHttpClient*>(this), response);
        }
        
        response->release();
    }
     
    if (0 == s_asyncRequestCount) 
    {
        cocos2d::CCDirector::sharedDirector()->getScheduler()->pauseTarget(this);
    }
    
}


}

#endif // CC_TARGET_PLATFORM != CC_PLATFORM_WINRT) && (CC_TARGET_PLATFORM != CC_PLATFORM_WP8)







