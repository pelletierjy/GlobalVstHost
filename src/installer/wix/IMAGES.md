# WiX Installer Images

The installer uses two banner images referenced in `product.wxs`:
- `banner.bmp` — 493×58 pixels (displayed at top of dialogs)
- `dialog.bmp` — 493×312 pixels (displayed on welcome screen)

## Option 1: Use WiX Default (Easiest for v1)

Comment out these lines in `product.wxs` to skip custom images and use WiX defaults:

```xml
<!-- <WixVariable Id="WixUIBannerBmp" Value="banner.bmp" /> -->
<!-- <WixVariable Id="WixUIDialogBmp" Value="dialog.bmp" /> -->
<!-- <WixVariable Id="WixUILicenseRtf" Value="license.rtf" /> -->
```

The installer will use WiX's built-in neutral blue theme.

## Option 2: Create Custom Images (v2+)

If you want branded images, create them as 24-bit BMP files:
1. Banner: 493×58 px
2. Dialog: 493×312 px

Save them in this directory (`src/installer/wix/`) and the CMake build will include them.

Tools to create BMPs:
- **ImageMagick**: `magick convert myimage.png -resize 493x58 banner.bmp`
- **GIMP**: Export → BMP (24-bit RGB)
- **Paint.NET**: File → Export As → BMP

## License File

`license.rtf` is embedded in the MSI. Update it with your actual license terms before release.
