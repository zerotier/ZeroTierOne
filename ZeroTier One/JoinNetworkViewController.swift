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

let joinedNetworksKey = "com.zerotier.one.joined-networks"


class JoinNetworkViewController: NSViewController, NSComboBoxDelegate, NSComboBoxDataSource {

    @IBOutlet var network: NSComboBox!
    @IBOutlet var joinButton: NSButton!

    @IBOutlet var allowManagedCheckBox: NSButton!
    @IBOutlet var allowGlobalCheckBox: NSButton!
    @IBOutlet var allowDefaultCheckBox:NSButton!

    var values: [String] = [String]()

    override func viewDidLoad() {
        super.viewDidLoad()
        network.setDelegate(self)
        network.dataSource = self
    }

    override func viewWillAppear() {
        super.viewWillAppear()

        allowManagedCheckBox.state = NSOnState
        allowGlobalCheckBox.state = NSOffState
        allowDefaultCheckBox.state = NSOffState

        let defaults = NSUserDefaults.standardUserDefaults()

        let vals = defaults.stringArrayForKey(joinedNetworksKey)

        if let v = vals {
            values = v
        }
    }

    override func viewDidDisappear() {
        super.viewWillDisappear()
        
        let defaults = NSUserDefaults.standardUserDefaults()

        defaults.setObject(values, forKey: joinedNetworksKey)
    }

    @IBAction func onJoinClicked(sender: AnyObject?) {
        let networkString = network.stringValue

        ServiceCom.sharedInstance().joinNetwork(networkString,
                                              allowManaged: allowManagedCheckBox.state == NSOnState,
                                              allowGlobal: allowGlobalCheckBox.state == NSOnState,
                                              allowDefault: allowDefaultCheckBox.state == NSOnState)
        network.stringValue = ""


        if !values.contains(networkString) {
            values.insert(networkString, atIndex: 0)

            while values.count > 20 {
                values.removeLast()
            }
        }
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


    // NSComboBoxDataSource methods

    func numberOfItemsInComboBox(aComboBox: NSComboBox) -> Int {
        return values.count
    }

    func comboBox(aComboBox: NSComboBox, objectValueForItemAtIndex index: Int) -> AnyObject {
        return values[index]
    }

    func comboBox(aComboBox: NSComboBox, indexOfItemWithStringValue string: String) -> Int {

        var counter = 0
        for val in values {
            if val == string {
                return counter
            }
            counter += 1
        }
        return NSNotFound
    }

    func comboBox(aComboBox: NSComboBox, completedString string: String) -> String? {
        for val in values {
            if val.hasPrefix(string) {
                return val
            }
        }

        return nil
    }

    // end NSComboBoxDataSorce methods
}
