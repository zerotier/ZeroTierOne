ZeroTier One
======

ZeroTier One creates flat virtual Ethernet networks of almost unlimited size. [Visit ZeroTier Networks on the web](https://www.zerotier.com/) for more information.

This code is presently in **ALPHA** testing. That means that the protocol spec may change in incompatible ways, and it certainly has bugs. Testers should "git pull," rebuild, and restart fairly often. If things mysteriously stop working, do that.

See BUILDING.txt and RUNNING.txt for instructions. It currently builds on Mac and Linux. A Windows port is well along the way to completion. If you're brave you can load the Visual Studio 2012 solution and play around.

Once you have it running you can join the Earth network -- a LAN for the planet -- by using the 'zerotier-cli' tool: 'sudo zerotier-cli join bc8f9a8ee3000001'. Right now Earth is the only network. Ability to create private networks, as well as some additional special-purpose public ones, will be coming soon. (Note: previous alpha versions auto-joined Earth, but since 0.5.0 it no longer does this.)

Note that this won't work if your firewall does not allow outbound UDP. It must allow two way UDP conversations on port 9993 at a minimum.

ZeroTier One is licensed under the GNU General Public License version 3. You are free to use, modify, or redistribute it under the terms of that license. If you would like to embed ZeroTier One in a closed source product or create a closed source derivative product, contact ZeroTier Networks LLC.

Follow the [ZeroTier blog](http://blog.zerotier.com/) for announcements, in-depth articles, and related stuff.

[![githalytics.com alpha](https://cruel-carlota.pagodabox.com/59b2cbb9c154bf84bddb4b714402e548 "githalytics.com")](http://githalytics.com/zerotier/ZeroTierOne)

(c)2012-2013 [ZeroTier Networks LLC](https://www.zerotier.com/)