#import "AppDelegate.h"
#include <tlmeta2/parser.c>

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    self.view.delegate = self;
    
    // init colors
    NSDictionary* base = @{
                                 @"": [NSColor blackColor],
                                 @"text": [NSColor brownColor],
                                 @"num": [NSColor brownColor],
                                 @"object": [NSColor brownColor],
                                 @"map": [NSColor brownColor],
                                 @"item": [NSColor brownColor],
                                 @"list": [NSColor brownColor],
                                 @"class": [NSColor brownColor],
                                 @"function": [NSColor brownColor],
                                 @"farg": [NSColor blueColor],
                                 @"literal": [NSColor redColor],
                                 @"ref": [NSColor blueColor],
                                 };
    
    self.colors = [[NSMutableArray alloc] init];
    for (int i = 0;; i++) {
        const char* color = colors[i];
        if (!color) break;
        id foo = @{NSForegroundColorAttributeName: [base objectForKey: [NSString stringWithUTF8String: color]]};
        [self.colors insertObject: foo atIndex:i];
    }
}

- (void)textDidChange:(NSNotification*)notification
{
    NSLog(@"change! %@", [self.view string]);
    const char* bytes = [[self.view string] UTF8String];
    
    Parser* p = parser_new(bytes, 0);
    bool r = parser_parse(p);
    if (!r) {
        NSLog(@"error: %s at: %d:%d", p->error_msg, p->error_line, p->error_char);
        return;
    }
    
    NSMutableAttributedString* text = [self.view textStorage];
    
    int begin = 0;
    int color = 0;
    for (int i = 0; i < p->len + 1; i++) {
        int c = p->out[i];
        
        if (color != c) {
            NSLog(@"color: %d %d %d", begin, i, color);
            [text setAttributes:[self.colors objectAtIndex:color] range:NSMakeRange(begin, i - begin)];
            begin = i;
            color = c;
        }
    }
    //[text fixAttributesInRange:NSMakeRange(0, begin - 1)];
}

@end