//
//  NetworkInfoCell.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/24/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class NetworkInfoCell: NSTableCellView {

    weak var parent: ShowNetworksViewController!

    @IBOutlet var networkIdField: NSTextField!
    @IBOutlet var networkNameField: NSTextField!

    @IBOutlet var statusField: NSTextField!
    @IBOutlet var typeField: NSTextField!
    @IBOutlet var macField: NSTextField!
    @IBOutlet var mtuField: NSTextField!
    @IBOutlet var broadcastField: NSTextField!
    @IBOutlet var bridgingField: NSTextField!
    @IBOutlet var deviceField: NSTextField!
    @IBOutlet var addressesField: NSTextField!

    @IBOutlet var connectedCheckbox: NSButton!
    @IBOutlet var deleteButton: NSButton!

    override func awakeFromNib() {
        super.awakeFromNib()
        
    }
    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)

        // Drawing code here.
    }

    @IBAction func onConnectCheckStateChanged(sender: NSButton) {
        NSLog("Checked State Changed")

        if(sender.state == NSOnState) {
            NSLog("Checked")
            joinNetwork(networkIdField.stringValue)
        }
        else {
            NSLog("Unchecked")
            leaveNetwork(networkIdField.stringValue)
        }
    }

    @IBAction func deleteNetwork(sender: NSButton) {
        leaveNetwork(networkIdField.stringValue)
        parent.deleteNetworkFromList(networkIdField.stringValue)
    }

    func joinNetwork(nwid: String) {
        ServiceCom.joinNetwork(nwid)
    }

    func leaveNetwork(nwid: String) {
        ServiceCom.leaveNetwork(nwid)
    }
}
