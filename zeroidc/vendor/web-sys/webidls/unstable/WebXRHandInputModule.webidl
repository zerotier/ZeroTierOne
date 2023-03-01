/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * WebXR Device API
 * W3C Working Draft, 19 April 2022
 * The origin of this IDL file is:
 * https://www.w3.org/TR/2022/WD-webxr-hand-input-1-20220419/
 */

partial interface XRInputSource {
   [SameObject] readonly attribute XRHand? hand;
};

enum XRHandJoint {
  "wrist",

  "thumb-metacarpal",
  "thumb-phalanx-proximal",
  "thumb-phalanx-distal",
  "thumb-tip",

  "index-finger-metacarpal",
  "index-finger-phalanx-proximal",
  "index-finger-phalanx-intermediate",
  "index-finger-phalanx-distal",
  "index-finger-tip",

  "middle-finger-metacarpal",
  "middle-finger-phalanx-proximal",
  "middle-finger-phalanx-intermediate",
  "middle-finger-phalanx-distal",
  "middle-finger-tip",

  "ring-finger-metacarpal",
  "ring-finger-phalanx-proximal",
  "ring-finger-phalanx-intermediate",
  "ring-finger-phalanx-distal",
  "ring-finger-tip",

  "pinky-finger-metacarpal",
  "pinky-finger-phalanx-proximal",
  "pinky-finger-phalanx-intermediate",
  "pinky-finger-phalanx-distal",
  "pinky-finger-tip"
};

[Exposed=Window]
interface XRHand {
    iterable<XRHandJoint, XRJointSpace>;

    readonly attribute unsigned long size;
    XRJointSpace get(XRHandJoint key);
};

[Exposed=Window]
interface XRJointSpace: XRSpace {
  readonly attribute XRHandJoint jointName;
};

partial interface XRFrame {
    XRJointPose? getJointPose(XRJointSpace joint, XRSpace baseSpace);
    boolean fillJointRadii(sequence<XRJointSpace> jointSpaces, Float32Array radii);

    boolean fillPoses(sequence<XRSpace> spaces, XRSpace baseSpace, Float32Array transforms);
};

[Exposed=Window]
interface XRJointPose: XRPose {
    readonly attribute float radius;
};
