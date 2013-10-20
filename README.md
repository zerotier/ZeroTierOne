ZeroTier One
======

ZeroTier One creates flat virtual Ethernet networks of almost unlimited size. [Visit ZeroTier Networks on the web](https://www.zerotier.com/) for more information.

This project is now in beta, which means that major incompatible protocol shifts should now become considerably more rare. You should still update very often if you plan to use it. Binary packages with auto-update are coming very soon.

Follow the [ZeroTier blog](http://blog.zerotier.com/) and the [GitHub project](https://github.com/zerotier/ZeroTierOne) to stay up to date.

See BUILDING.txt and RUNNING.txt for instructions. It currently runs on Mac and Linux. A Windows port is well along the way to completion. If you're brave you can load the Visual Studio 2012 solution and play around.

Once you have it running you can join the Earth network -- a LAN for the planet -- by using the 'zerotier-cli' tool: 'sudo zerotier-cli join 8056c2e21c000001'. Right now Earth is the only network. Ability to create private networks, as well as some additional special-purpose public ones, will be coming soon. (Note: previous alpha versions auto-joined Earth, but since 0.5.0 it no longer does this.)

Note that this won't work if your firewall does not allow outbound UDP. It must allow two way UDP conversations on port 9993 at a minimum.

ZeroTier One is licensed under the GNU General Public License version 3. You are free to use, modify, or redistribute it under the terms of that license. If you would like to embed ZeroTier One in a closed source product or create a closed source derivative product, contact ZeroTier Networks LLC.

(c)2012-2013 [ZeroTier Networks LLC](https://www.zerotier.com/)