//
//  AppDelegate.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/14/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    @IBOutlet weak var window: NSWindow!


    let statusItem = NSStatusBar.systemStatusBar().statusItemWithLength(-2.0)

    let networkListPopover = NSPopover()
    let joinNetworkPopover = NSPopover()
    let preferencesPopover = NSPopover()

    var transientMonitor: AnyObject? = nil

    let monitor = NetworkMonitor()

    var networks = [Network]()

    func applicationDidFinishLaunching(aNotification: NSNotification) {
        let nc = NSNotificationCenter.defaultCenter()
        nc.addObserver(self, selector: #selector(onNetworkListUpdated(_:)), name: networkUpdateKey, object: nil)

        statusItem.image = NSImage(named: "MenuBarIconMac")

        buildMenu()

        joinNetworkPopover.contentViewController = JoinNetworkViewController(
            nibName: "JoinNetworkViewController", bundle: nil)
        joinNetworkPopover.behavior = .Transient

        joinNetworkPopover.appearance = NSAppearance(named: NSAppearanceNameAqua)

        networkListPopover.contentViewController = ShowNetworksViewController(
            nibName: "ShowNetworksViewController", bundle: nil)
        networkListPopover.behavior = .Transient

        networkListPopover.appearance = NSAppearance(named: NSAppearanceNameAqua)

        preferencesPopover.contentViewController = PreferencesViewController(
            nibName: "PreferencesViewController", bundle: nil)
        preferencesPopover.behavior = .Transient

        preferencesPopover.appearance = NSAppearance(named: NSAppearanceNameAqua)
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

    func quit() {
        NSApp.performSelector(#selector(NSApp.terminate(_:)), withObject: nil, afterDelay: 0.0)
    }

    func onNetworkListUpdated(note: NSNotification) {
        let netList = note.userInfo!["networks"] as! [Network]
        (networkListPopover.contentViewController as! ShowNetworksViewController).setNetworks(netList)

        self.networks = netList

        buildMenu()
    }

    func buildMenu() {
        let menu = NSMenu()

        menu.addItem(NSMenuItem(title: "Network Details", action: #selector(AppDelegate.showNetworks), keyEquivalent: "n"))
        menu.addItem(NSMenuItem(title: "Join Network", action: #selector(AppDelegate.joinNetwork), keyEquivalent: "j"))
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
            ServiceCom.leaveNetwork(id)
        }
        else {
            ServiceCom.joinNetwork(id)
        }
    }
}

