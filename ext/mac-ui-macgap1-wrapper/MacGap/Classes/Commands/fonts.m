#import "fonts.h"

@implementation Fonts


- (NSArray*) availableFonts {
    return [[NSFontManager sharedFontManager] availableFonts];
}

- (NSArray*) availableFontFamilies {
    return [[NSFontManager sharedFontManager] availableFontFamilies];
}

- (NSArray*) availableMembersOfFontFamily:(NSString *)fontFamily {
    return [[NSFontManager sharedFontManager] availableMembersOfFontFamily:fontFamily];
}

- (CGFloat) defaultLineHeightForFont:(NSString*)theFontName ofSize:(CGFloat)theFontSize {
    NSFont *theFont = [NSFont fontWithName:theFontName size:theFontSize];
    NSLayoutManager *lm = [[NSLayoutManager alloc] init];
    
    return [lm defaultLineHeightForFont:theFont];
}


#pragma mark WebScripting Protocol

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector {
    return NO;
}

+ (NSString*) webScriptNameForSelector:(SEL)selector {
	id	result = nil;
	
	if (selector == @selector(availableMembersOfFontFamily:)) {
		result = @"availableMembersOfFontFamily";
    } else if (selector == @selector(defaultLineHeightForFont:ofSize:)) {
        result = @"defaultLineHeightForFont";
    }

	return result;
}

+ (BOOL) isKeyExcludedFromWebScript:(const char*)name {
	return NO;
}

@end
