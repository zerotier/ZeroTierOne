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

    var transientMonitor: AnyObject? = nil

    func applicationDidFinishLaunching(aNotification: NSNotification) {


        statusItem.image = NSImage(named: "MenuBarIconMac")

        let menu = NSMenu()

        menu.addItem(NSMenuItem(title: "Show Networks", action: #selector(AppDelegate.showNetworks), keyEquivalent: "n"))
        menu.addItem(NSMenuItem(title: "Join Network", action: #selector(AppDelegate.joinNetwork), keyEquivalent: "j"))
        menu.addItem(NSMenuItem.separatorItem())
        menu.addItem(NSMenuItem(title: "Quit ZeroTier One", action: #selector(AppDelegate.quit), keyEquivalent: "q"))

        statusItem.menu = menu

        joinNetworkPopover.contentViewController = JoinNetworkViewController(
            nibName: "JoinNetworkViewController", bundle: nil)
        joinNetworkPopover.behavior = .Transient

        joinNetworkPopover.appearance = NSAppearance(named: NSAppearanceNameAqua)

        networkListPopover.contentViewController = ShowNetworksViewController(
            nibName: "ShowNetworksViewController", bundle: nil)
        networkListPopover.behavior = .Transient

        networkListPopover.appearance = NSAppearance(named: NSAppearanceNameAqua)
    }

    func applicationWillTerminate(aNotification: NSNotification) {
        // Insert code here to tear down your application
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

    func quit() {
        NSApp.performSelector(#selector(NSApp.terminate(_:)), withObject: nil, afterDelay: 0.0)
    }
}

