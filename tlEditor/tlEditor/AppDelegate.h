//
//  AppDelegate.h
//  tlEditor
//
//  Created by ogorter on 29/12/13.
//  Copyright (c) 2013 ogorter. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate, NSTextViewDelegate>

@property (assign) IBOutlet NSWindow* window;
@property (assign) IBOutlet NSTextView* view;
@property (retain) NSMutableArray* colors;
@property (retain) id light_error_markup;
@property (retain) id dark_error_markup;
@end

