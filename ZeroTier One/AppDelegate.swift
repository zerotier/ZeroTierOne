//
//  AppDelegate.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/14/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate {

    @IBOutlet weak var window: NSWindow!


    let statusItem = NSStatusBar.systemStatusBar().statusItemWithLength(-2.0)

    let networkListPopover = NSPopover()
    let joinNetworkPopover = NSPopover()
    let preferencesPopover = NSPopover()
    let aboutPopover = NSPopover()

    var transientMonitor: AnyObject? = nil

    let monitor = NetworkMonitor()

    var networks = [Network]()

    var status: NodeStatus? = nil

    var pasteboard = NSPasteboard.generalPasteboard()

    func applicationDidFinishLaunching(aNotification: NSNotification) {
        pasteboard.declareTypes([NSPasteboardTypeString], owner: nil)

        let defaults = NSUserDefaults.standardUserDefaults()
        let defaultsDict = ["firstRun": true]
        defaults.registerDefaults(defaultsDict)




        let nc = NSNotificationCenter.defaultCenter()
        nc.addObserver(self, selector: #selector(onNetworkListUpdated(_:)), name: networkUpdateKey, object: nil)
        nc.addObserver(self, selector: #selector(onNodeStatusUpdated(_:)), name: statusUpdateKey, object: nil)

        statusItem.image = NSImage(named: "MenuBarIconMac")

        buildMenu()

        joinNetworkPopover.contentViewController = JoinNetworkViewController(
            nibName: "JoinNetworkViewController", bundle: nil)
        joinNetworkPopover.behavior = .Transient

        let showNetworksView = ShowNetworksViewController(
            nibName: "ShowNetworksViewController", bundle: nil)
        showNetworksView?.netMonitor = monitor

        networkListPopover.contentViewController = showNetworksView
        networkListPopover.behavior = .Transient

        let prefsView = PreferencesViewController(nibName: "PreferencesViewController", bundle: nil)
        preferencesPopover.contentViewController = prefsView
        preferencesPopover.behavior = .Transient

        aboutPopover.contentViewController = AboutViewController(
            nibName: "AboutViewController", bundle: nil)
        aboutPopover.behavior = .Transient

        let firstRun = defaults.boolForKey("firstRun")

        if firstRun {
            defaults.setBool(false, forKey: "firstRun")
            defaults.synchronize()

            prefsView?.setLaunchAtLoginEnabled(true)
        }

        if firstRun {
            NSOperationQueue.mainQueue().addOperationWithBlock() {
                self.showAbout()
            }
        }

        monitor.updateNetworkInfo()
        monitor.start()
    }

    func applicationWillTerminate(aNotification: NSNotification) {
        // Insert code here to tear down your application
        let nc = NSNotificationCenter.defaultCenter()
        nc.removeObserver(self)
    }


    func showNetworks() {
        if let button = statusItem.button {
            networkListPopover.showRelativeToRect(button.bounds, ofView: button, preferredEdge: .MinY)

            if transientMonitor == nil {
                transientMonitor = NSEvent.addGlobalMonitorForEventsMatchingMask(
                    [.LeftMouseDownMask, .RightMouseDownMask, .OtherMouseDownMask]) { (event: NSEvent) -> Void in

                        NSEvent.removeMonitor(self.transientMonitor!)
                        self.transientMonitor = nil
                        self.networkListPopover.close()
                }
            }
        }
    }

    func joinNetwork() {
        if let button = statusItem.button {
            joinNetworkPopover.showRelativeToRect(button.bounds, ofView: button, preferredEdge: .MinY)

            if transientMonitor == nil {
                transientMonitor = NSEvent.addGlobalMonitorForEventsMatchingMask(
                    [.LeftMouseDownMask, .RightMouseDownMask, .OtherMouseDownMask]) { (event: NSEvent) -> Void in

                        NSEvent.removeMonitor(self.transientMonitor!)
                        self.transientMonitor = nil
                        self.joinNetworkPopover.close()

                }
            }
        }
    }

    func showPreferences() {
        if let button = statusItem.button {
            preferencesPopover.showRelativeToRect(button.bounds, ofView: button, preferredEdge: .MinY)

            if transientMonitor == nil {
                transientMonitor = NSEvent.addGlobalMonitorForEventsMatchingMask(
                    [.LeftMouseDownMask, .RightMouseDownMask, .OtherMouseDownMask]) { (event: NSEvent) -> Void in

                        NSEvent.removeMonitor(self.transientMonitor!)
                        self.transientMonitor = nil
                        self.preferencesPopover.close()
                }
            }
        }
    }

    func showAbout() {
        if let button = statusItem.button {
            aboutPopover.showRelativeToRect(button.bounds, ofView: button, preferredEdge: .MinY)

            if transientMonitor == nil {
                transientMonitor = NSEvent.addGlobalMonitorForEventsMatchingMask(
                    [.LeftMouseDownMask, .RightMouseDownMask, .OtherMouseDownMask]) { (event: NSEvent) -> Void in

                        NSEvent.removeMonitor(self.transientMonitor!)
                        self.transientMonitor = nil
                        self.aboutPopover.close()
                }
            }
        }
    }

    func quit() {
        NSApp.performSelector(#selector(NSApp.terminate(_:)), withObject: nil, afterDelay: 0.0)
    }

    func onNetworkListUpdated(note: NSNotification) {
        let netList = note.userInfo!["networks"] as! [Network]
        (networkListPopover.contentViewController as! ShowNetworksViewController).setNetworks(netList)

        self.networks = netList

        buildMenu()
    }

    func onNodeStatusUpdated(note: NSNotification) {
        let status = note.userInfo!["status"] as! NodeStatus
        self.status = status

        buildMenu()
    }

    func buildMenu() {
        let menu = NSMenu()
        menu.delegate = self

        if let s = self.status {
            menu.addItem(NSMenuItem(title: "Node ID: \(s.address)", action: #selector(AppDelegate.copyNodeID), keyEquivalent: ""))
            menu.addItem(NSMenuItem.separatorItem())
        }

        menu.addItem(NSMenuItem(title: "Network Details...", action: #selector(AppDelegate.showNetworks), keyEquivalent: "n"))
        menu.addItem(NSMenuItem(title: "Join Network...", action: #selector(AppDelegate.joinNetwork), keyEquivalent: "j"))
        menu.addItem(NSMenuItem.separatorItem())

        if networks.count > 0 {
            for net in networks {
                let id = String(net.nwid, radix: 16)
                var networkName = ""
                if net.name.isEmpty {
                    networkName = "\(id)"
                }
                else {
                    networkName = "\(id) (\(net.name))"
                }

                if net.allowDefault && net.connected {
                    networkName += " [default]"
                }
                let item = NSMenuItem(title: networkName, action: #selector(AppDelegate.toggleNetwork(_:)), keyEquivalent: "")

                if net.connected {
                    item.state = NSOnState
                }
                else {
                    item.state = NSOffState
                }

                item.representedObject = net

                menu.addItem(item)
            }

            menu.addItem(NSMenuItem.separatorItem())
        }

        menu.addItem(NSMenuItem(title: "About ZeroTier One...", action: #selector(AppDelegate.showAbout), keyEquivalent: ""))
        menu.addItem(NSMenuItem(title: "Preferences...", action: #selector(AppDelegate.showPreferences), keyEquivalent: ","))

        menu.addItem(NSMenuItem.separatorItem())

        menu.addItem(NSMenuItem(title: "Quit ZeroTier One", action: #selector(AppDelegate.quit), keyEquivalent: "q"))

        statusItem.menu = menu
    }

    func toggleNetwork(sender: NSMenuItem) {
        NSLog("\(sender.title)")

        let network = sender.representedObject as! Network

        let id = String(network.nwid, radix: 16)

        if network.connected {
            ServiceCom.sharedInstance().leaveNetwork(id)
        }
        else {
            ServiceCom.sharedInstance().joinNetwork(id, allowManaged: true, allowGlobal: false, allowDefault: false)
        }
    }

    func copyNodeID() {
        if let s = self.status {
            pasteboard.setString(s.address, forType: NSPasteboardTypeString)
        }
    }

    func menuWillOpen(menu: NSMenu) {
        //monitor.updateNetworkInfo()
    }

    func menuDidClose(menu: NSMenu) {
    }
}

