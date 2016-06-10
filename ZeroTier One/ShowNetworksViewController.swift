//
//  ShowNetworksViewController.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/14/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class ShowNetworksViewController: NSViewController, NSTableViewDelegate, NSTableViewDataSource {

    @IBOutlet var tableView: NSTableView!

    var networkList: [Network] = [Network]()

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.

        tableView.setDelegate(self)
        tableView.setDataSource(self)
        tableView.backgroundColor = NSColor.clearColor()
    }

    override func viewWillAppear() {
        super.viewWillAppear()

        ServiceCom.getNetworkList() { (networkList) -> Void in
            NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                self.networkList = networkList
                self.tableView.reloadData()
            }
        }
    }

    func deleteNetworkFromList(nwid: String) {
        
    }

    // NSTableViewDataSource

    func numberOfRowsInTableView(tableView: NSTableView) -> Int {
        return networkList.count
    }

    // end NSTableViewDataSource

    // NSTableViewDelegate

    func tableView(tableView: NSTableView, viewForTableColumn tableColumn: NSTableColumn?, row: Int) -> NSView? {
        if let cell = tableView.makeViewWithIdentifier("NetworkInfoCell", owner: nil) as? NetworkInfoCell {
            let network = networkList[row]
            cell.parent = self
            cell.networkIdField.stringValue = String(network.nwid, radix: 16)
            cell.networkNameField.stringValue = network.name

            cell.statusField.stringValue = network.status.description
            cell.typeField.stringValue = network.type.description
            cell.macField.stringValue = network.mac
            cell.mtuField.stringValue = String(network.mtu)
            cell.broadcastField.stringValue = network.broadcastEnabled ? "ENABLED" : "DISABLED"
            cell.bridgingField.stringValue = network.bridge ? "ENABLED" : "DISABLED"
            cell.deviceField.stringValue = network.portDeviceName


            cell.addressesField.stringValue = ""

            for nw in network.assignedAddresses {
                cell.addressesField.stringValue += nw
                cell.addressesField.stringValue += "\n"
            }


            return cell
        }

        return nil
    }

    // end NSTableViewDelegate
}
