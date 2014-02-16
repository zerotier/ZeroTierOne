ZeroTier One
======

ZeroTier One creates flat virtual Ethernet networks of almost unlimited size. [Visit ZeroTier Networks on the web](https://www.zerotier.com/) for more information.

Follow the [ZeroTier blog](http://blog.zerotier.com/) and the [GitHub project](https://github.com/zerotier/ZeroTierOne) to stay up to date.

Auto-updating binary packages that install easily [can be found here.](https://www.zerotier.com/downloads.html) If you want to build from source, clone this repository and see BUILDING.txt and RUNNING.txt.

Once you are up and running, you can [create an account on the network control panel](https://www.zerotier.com/networks.html) if you want to create a private network or you can join [Earth](https://www.zerotier.com/earth.html) by running (on Linux and Mac) `sudo ./zerotier-cli join 8056c2e21c000001`.

Note that this won't work if your firewall does not allow outbound UDP. It must allow two way UDP conversations on port 9993 at a minimum.

ZeroTier One is licensed under the GNU General Public License version 3. You are free to use, modify, or redistribute it under the terms of that license. If you would like to embed ZeroTier One in a closed source product or create a closed source derivative product, contact ZeroTier Networks LLC.

(c)2012-2013 [ZeroTier Networks LLC](https://www.zerotier.com/)