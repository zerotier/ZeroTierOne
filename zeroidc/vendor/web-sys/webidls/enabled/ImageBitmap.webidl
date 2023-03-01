/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/webappapis.html#images
 */

typedef (CanvasImageSource or
         Blob or
         ImageData) ImageBitmapSource;

[Exposed=(Window,Worker)]
interface ImageBitmap {
  [Constant]
  readonly attribute unsigned long width;
  [Constant]
  readonly attribute unsigned long height;
};

// It's crucial that there be a way to explicitly dispose of ImageBitmaps
// since they refer to potentially large graphics resources. Some uses
// of this API proposal will result in repeated allocations of ImageBitmaps,
// and garbage collection will not reliably reclaim them quickly enough.
// Here we reuse close(), which also exists on another Transferable type,
// MessagePort. Potentially, all Transferable types should inherit from a
// new interface type "Closeable".
partial interface ImageBitmap {
  // Dispose of all graphical resources associated with this ImageBitmap.
  undefined close();
};

enum ImageOrientation { "from-image", "flipY" };
enum PremultiplyAlpha { "none", "premultiply", "default" };
enum ColorSpaceConversion { "none", "default" };
enum ResizeQuality { "pixelated", "low", "medium", "high" };

dictionary ImageBitmapOptions {
  ImageOrientation imageOrientation = "from-image";
  PremultiplyAlpha premultiplyAlpha = "default";
  ColorSpaceConversion colorSpaceConversion = "default";
  [EnforceRange] unsigned long resizeWidth;
  [EnforceRange] unsigned long resizeHeight;
  ResizeQuality resizeQuality = "low";
};
