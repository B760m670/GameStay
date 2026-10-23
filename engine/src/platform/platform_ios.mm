// iOS: пути к ресурсам бандла и к Documents.
#import <Foundation/Foundation.h>
#include "platform.h"

namespace platform {
std::string assetDir() {
    NSString* p = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"assets"];
    return std::string([p UTF8String]);
}
std::string saveDir() {
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    return std::string([[paths firstObject] UTF8String]);
}
}

// ---------- сеть: NSURLSession (HTTPS) и NSURLSessionWebSocketTask ----------
#include <atomic>
#include "../engine/net.h"
#include "sokol_app.h"

namespace platform {
namespace {
NSURLSessionWebSocketTask* gWs = nil;
std::atomic<int> gWsGen{0};

NSString* ns(const std::string& s) { return [NSString stringWithUTF8String:s.c_str()]; }

void wsReceive(NSURLSessionWebSocketTask* t, int gen) {
    [t receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage* m, NSError* err) {
        if (gen != gWsGen.load()) return;
        if (err) { net::wsDeliver(2, ""); return; }
        if (m.type == NSURLSessionWebSocketMessageTypeString && m.string) net::wsDeliver(0, std::string(m.string.UTF8String));
        wsReceive(t, gen);
    }];
}
}  // namespace

void httpRequest(int rid, const std::string& method, const std::string& url, const std::string& body, const std::string& token) {
    NSURL* u = [NSURL URLWithString:ns(url)];
    if (!u) { net::deliver(rid, 0, ""); return; }
    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:u];
    req.HTTPMethod = ns(method);
    req.timeoutInterval = 20;
    [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    if (!token.empty()) [req setValue:[@"Bearer " stringByAppendingString:ns(token)] forHTTPHeaderField:@"Authorization"];
    if (!body.empty()) req.HTTPBody = [NSData dataWithBytes:body.data() length:body.size()];
    NSURLSessionDataTask* task = [[NSURLSession sharedSession] dataTaskWithRequest:req
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
            int status = 0;
            if (!err && [resp isKindOfClass:[NSHTTPURLResponse class]]) status = (int)((NSHTTPURLResponse*)resp).statusCode;
            std::string b = data ? std::string((const char*)data.bytes, (size_t)data.length) : std::string();
            net::deliver(rid, status, b);
        }];
    [task resume];
}

void wsOpen(const std::string& url) {
    wsClose();
    int gen = ++gWsGen;
    NSURL* u = [NSURL URLWithString:ns(url)];
    if (!u) { net::wsDeliver(2, ""); return; }
    gWs = [[NSURLSession sharedSession] webSocketTaskWithURL:u];
    [gWs resume];
    NSURLSessionWebSocketTask* t = gWs;
    // явного «подключено» без делегата нет — пинг ответит, когда соединение готово
    [t sendPingWithPongReceiveHandler:^(NSError* e) {
        if (gen == gWsGen.load()) net::wsDeliver(e ? 2 : 1, "");
    }];
    wsReceive(t, gen);
}

void wsSend(const std::string& text) {
    if (!gWs) return;
    NSURLSessionWebSocketMessage* m = [[NSURLSessionWebSocketMessage alloc] initWithString:ns(text)];
    [gWs sendMessage:m completionHandler:^(NSError* e) {}];
}

void wsClose() {
    if (gWs) {
        [gWs cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];
        gWs = nil;
    }
    ++gWsGen;
}

void showKeyboard(bool show) { sapp_show_keyboard(show); }
}  // namespace platform
