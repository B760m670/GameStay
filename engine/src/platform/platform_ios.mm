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
