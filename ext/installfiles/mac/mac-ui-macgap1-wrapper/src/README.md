Mac Web UI Wrapper
======

This is a modified version of MacGap1 which launches a WebKit view and accesses the local ZeroTier service at its web URL. It builds the URL from the authtoken.secret file in the system home (or the user home) and the zerotier-one.port file that ZeroTier creates to advertise its control port.

It's based on the original MacGap1 source by Twitter, Inc. which is licensed under the MIT license.
