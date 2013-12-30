#import "AppDelegate.h"
#include <tlmeta2/parser.c>

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    self.view.delegate = self;
    self.view.automaticQuoteSubstitutionEnabled = NO;
    [self.view setString:@"#hotel example syntax\nmethods: {\n  move: dx, dy -> this::copy({ x: x + dx, y: y + dy })\n  string: -> \"$x,$y\"\n}\n"];

    // init colors
    NSDictionary* base = @{
                                 @"": [NSColor darkGrayColor],
                                 @"slcomment": [NSColor grayColor],
                                 @"comment": [NSColor grayColor],
                                 @"literal": [NSColor brownColor],
                                 @"text": [NSColor brownColor],
                                 @"num": [NSColor brownColor],
                                 @"key": [NSColor blackColor],
                                 @"farg": [NSColor blackColor],
                                 @"intro": [NSColor blackColor],
                                 @"method": [NSColor blueColor],
                                 @"ref": [NSColor blueColor],
                                 };
    
    self.light_error_markup = @{NSBackgroundColorAttributeName: [NSColor colorWithCalibratedRed:1 green:0.9 blue:0.9 alpha:1]};
    self.dark_error_markup = @{NSBackgroundColorAttributeName: [NSColor colorWithCalibratedRed:1 green:0.7 blue:0.7 alpha:1]};
    
    self.colors = [[NSMutableArray alloc] init];
    for (int i = 0;; i++) {
        const char* color = colors[i];
        if (!color) break;
        id c = [base objectForKey: [NSString stringWithUTF8String: color]];
        if (!c) c = [NSColor blackColor];
        [self.colors insertObject:@{NSForegroundColorAttributeName: c} atIndex:i];
    }
    
    [self textDidChange:nil];
}

- (void)textDidChange:(NSNotification*)notification
{
    NSTextStorage* text = [self.view textStorage];
    const char* bytes = [[text string] UTF8String];
    
    Parser* p = parser_new(bytes, 0);
    bool r = parser_parse(p);
    
    int begin = 0;
    int color = 0;
    int at = 0;
    for (; at < p->len; at++) {
        int c = p->out[at];
        if (color != c) {
            [text setAttributes:[self.colors objectAtIndex:color] range:NSMakeRange(begin, at - begin)];
            begin = at;
            color = c;
        }
    }
    
    if (!r) {
        NSLog(@"error: %s at: %d:%d", p->error_msg, p->error_line, p->error_char);
        [text addAttributes:self.light_error_markup range:NSMakeRange(p->error_line_begin, p->error_line_end - p->error_line_begin)];
        int begin = p->error_pos_begin;
        int len = p->error_pos_end - p->error_pos_begin;
        if (len <= 0) {
            len = 1;
            begin -= 1;
            if (begin < 0) begin = 0;
            while (begin + len > p->len) len--;
        }
        [text addAttributes:self.dark_error_markup range:NSMakeRange(begin, len)];
    }
    
    [text setAttributes:[self.colors objectAtIndex:color] range:NSMakeRange(begin, at - begin)];
    [text fixAttributesInRange:NSMakeRange(0, p->len)];
    [text setFont:[NSFont fontWithName:@"Menlo" size:14]];
}

@end