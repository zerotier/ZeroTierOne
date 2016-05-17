//
//  JoinNetworkViewController.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/14/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

extension String {
    func contains(find: String) -> Bool {
        return self.rangeOfString(find) != nil
    }

    func trunc(length: Int, trailing: String? = "...") -> String {
        if self.characters.count > length {
            return self.substringToIndex(self.startIndex.advancedBy(length)) + (trailing ?? "")
        } else {
            return self
        }
    }
}

class JoinNetworkViewController: NSViewController, NSComboBoxDelegate {

    @IBOutlet var network: NSComboBox!
    @IBOutlet var joinButton: NSButton!

    override func viewDidLoad() {
        super.viewDidLoad()
        network.setDelegate(self)
    }

    @IBAction func onJoinClicked(sender: AnyObject?) {
        let networkId = UInt64(network.stringValue, radix: 16)

        // TODO: Execute join network call

        network.stringValue = ""
    }


    // NSComboBoxDelegate Methods

    override func controlTextDidChange(obj: NSNotification) {
        let cb = obj.object as! NSComboBox
        let value = cb.stringValue


        let allowedCharacters = "abcdefABCDEF0123456789"

        var outValue = ""

        for char in value.characters {
            if !allowedCharacters.contains(String(char)) {
                NSBeep()
            }
            else {
                outValue += String(char)
            }
        }



        if outValue.lengthOfBytesUsingEncoding(NSUTF8StringEncoding) == 16 {
            joinButton.enabled = true
        }
        else {

            if outValue.lengthOfBytesUsingEncoding(NSUTF8StringEncoding) > 16 {
                outValue = outValue.trunc(16, trailing: "")
                NSBeep()
                joinButton.enabled = true
            }
            else {
                joinButton.enabled = false
            }
        }

        cb.stringValue = outValue
    }
    // end NSComboBoxDelegate Methods
}
