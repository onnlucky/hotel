//
//  main.m
//  tlEditor
//
//  Created by ogorter on 29/12/13.
//  Copyright (c) 2013 ogorter. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <vm/tl.h>

int main(int argc, const char * argv[])
{
    tl_init();
    return NSApplicationMain(argc, argv);
}
