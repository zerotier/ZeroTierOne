//
//  PreferencesViewController.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 6/23/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class PreferencesViewController: NSViewController {

    @IBOutlet var startupCheckBox: NSButton!

    let launchController = LaunchAtLoginController()

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.

        if launchController.launchAtLogin {
            startupCheckBox.state = NSOnState
        }
        else {
            startupCheckBox.state = NSOffState
        }
    }

    @IBAction func onStartupCheckBoxChanged(sender: NSButton) {
        let bundle = NSBundle.mainBundle()
        let bundleURL = bundle.bundleURL

        if sender.state == NSOnState {
            launchController.setLaunchAtLogin(true, forURL: bundleURL)
        }
        else {
            launchController.setLaunchAtLogin(false, forURL: bundleURL)
        }
    }
    
}
