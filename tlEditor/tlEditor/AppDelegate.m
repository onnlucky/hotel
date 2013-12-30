#import "AppDelegate.h"
#include <tlmeta2/parser.c>

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    self.view.delegate = self;
    self.colors = @[
                        @{NSForegroundColorAttributeName: [NSColor blueColor]},
                        @{NSForegroundColorAttributeName: [NSColor brownColor]},
                        @{NSForegroundColorAttributeName: [NSColor cyanColor]},
                        @{NSForegroundColorAttributeName: [NSColor darkGrayColor]},
                        @{NSForegroundColorAttributeName: [NSColor grayColor]},
                        @{NSForegroundColorAttributeName: [NSColor greenColor]},
                        @{NSForegroundColorAttributeName: [NSColor lightGrayColor]},
                        @{NSForegroundColorAttributeName: [NSColor magentaColor]},
                        @{NSForegroundColorAttributeName: [NSColor orangeColor]},
                        @{NSForegroundColorAttributeName: [NSColor purpleColor]},
                        @{NSForegroundColorAttributeName: [NSColor redColor]},
                        @{NSForegroundColorAttributeName: [NSColor yellowColor]},
                        
                        @{NSForegroundColorAttributeName: [NSColor blueColor]},
                        @{NSForegroundColorAttributeName: [NSColor brownColor]},
                        @{NSForegroundColorAttributeName: [NSColor cyanColor]},
                        @{NSForegroundColorAttributeName: [NSColor darkGrayColor]},
                        @{NSForegroundColorAttributeName: [NSColor grayColor]},
                        @{NSForegroundColorAttributeName: [NSColor greenColor]},
                        @{NSForegroundColorAttributeName: [NSColor lightGrayColor]},
                        @{NSForegroundColorAttributeName: [NSColor magentaColor]},
                        @{NSForegroundColorAttributeName: [NSColor orangeColor]},
                        @{NSForegroundColorAttributeName: [NSColor purpleColor]},
                        @{NSForegroundColorAttributeName: [NSColor redColor]},
                        @{NSForegroundColorAttributeName: [NSColor yellowColor]},
    ];
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
    for (int i = 0; i < p->len; i++) {
        int c = p->out[i];
        
        if (color && color != c) {
            [text setAttributes:[self.colors objectAtIndex:color] range:NSMakeRange(begin, i - begin)];
            begin = i;
            color = c;
        }
        if (!c) {
            begin = i;
        } else {
            color = c;
        }
    }
}

@end