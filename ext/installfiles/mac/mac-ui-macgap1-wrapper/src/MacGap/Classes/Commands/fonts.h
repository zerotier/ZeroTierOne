@interface Fonts : NSObject {
}

- (NSArray*) availableFonts;
- (NSArray*) availableFontFamilies;
- (NSArray*) availableMembersOfFontFamily:(NSString*)fontFamily;
- (CGFloat)  defaultLineHeightForFont:(NSString *)theFontName ofSize:(CGFloat)theFontSize;

@end
