#include "CCJsonTools.hpp"
#include "ProxyManager.hpp"

#include <pthread/pthread.h>
#include <memory>


/* A Global Handle for Proxy Management and settings */
ProxyManager *Global_ProxyManager = nullptr;


void ProxyManager::encodeDataTo(CCJsonReader* reader) {
    proxy_url = reader->getString("proxy_url");
    enabled = reader->getBool("enable");
}

void ProxyManager::saveDataTo(CCJsonWriter* writer){
    writer->setStr("proxy_url", proxy_url);
    writer->setBool("enabled", enabled);
}

void ProxyManager::firstload(){
    proxy_url = "";
    enabled = false;
}

/* Brings back the allocated Global_proxy_manager if it exists... */
ProxyManager* ProxyManager::sharedState(){
    if (Global_ProxyManager == nullptr){
        Global_ProxyManager = new ProxyManager();
        /* Whenever in scope we need to bring back out mutex to prevent the 
        proxy from ripping itself mid-request */
        pthread_mutex_init(&Global_ProxyManager->mutex, nullptr);
        Global_ProxyManager->setup();
    }
    return Global_ProxyManager;
};

ProxyManager::~ProxyManager(){
    save();
    /* Release Request handles... */
    m_download->release();
    pthread_mutex_destroy(&mutex);
}



std::string ProxyManager::getProxy(){
    pthread_mutex_lock(&mutex);
    std::string __copy(proxy_url); 
    pthread_mutex_unlock(&mutex);
    return __copy;
}

void ProxyManager::setProxy(std::string proxy){
    pthread_mutex_lock(&mutex);
    proxy_url = proxy;
    pthread_mutex_unlock(&mutex);
}


/* Used mainly for testing HTTP Requests with a proxy to make sure it works correctly... */
void ProxyManager::ProcessHttpRequest(std::string_view endpoint, std::string params, std::string_view tag){
    
};
