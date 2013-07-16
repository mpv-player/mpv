#import <CoreServices/CoreServices.h>
#import <OpenGL/OpenGL.h>
#import <Cocoa/Cocoa.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        NSArray *ary = @[@1, @2, @3];
        NSLog(@"test subscripting: %@", ary[0]);
        NSApplicationLoad();
    }
}
