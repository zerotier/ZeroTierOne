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

    private func  dataFile() -> String {
        var appSupport = NSFileManager.defaultManager().URLsForDirectory(.ApplicationSupportDirectory, inDomains: .UserDomainMask)[0]
        appSupport = appSupport.URLByAppendingPathComponent("ZeroTier").URLByAppendingPathComponent("One").URLByAppendingPathComponent("networks.dat")
        return appSupport.path!
    }


    private func findNetworkWithID(id: UInt64) -> Int {

        for (index, element) in networkList.enumerate() {

            if element.nwid == id {
                return index
            }
        }

        return NSNotFound
    }

    override func viewWillAppear() {
        super.viewWillAppear()

        let filePath = dataFile()

        if NSFileManager.defaultManager().fileExistsAtPath(filePath) {
            networkList = NSKeyedUnarchiver.unarchiveObjectWithFile(filePath) as! [Network]

            ServiceCom.getNetworkList() { (networkList) -> Void in

                for nw in networkList {
                    let index = self.findNetworkWithID(nw.nwid)

                    if index != NSNotFound {
                        self.networkList[index] = nw
                    }
                    else {
                        self.networkList.append(nw)
                    }
                }

                NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                    self.networkList.sortInPlace({ (left, right) -> Bool in
                        if left.nwid < right.nwid {
                            return true
                        }

                        return false
                    })
                    self.tableView.reloadData()
                }
            }
        }
        else {
            ServiceCom.getNetworkList() { (networkList) -> Void in
                NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                    self.networkList.sortInPlace({ (left, right) -> Bool in
                        if left.nwid < right.nwid {
                            return true
                        }

                        return false
                    })
                    
                    self.networkList = networkList
                    self.tableView.reloadData()
                }
            }
        }
    }

    override func viewWillDisappear() {
        super.viewWillDisappear()

        let filePath = dataFile()
        NSKeyedArchiver.archiveRootObject(self.networkList, toFile: filePath)
    }

    func deleteNetworkFromList(nwid: String) {
        if let nwid = UInt64(nwid, radix: 16) {
            let index = findNetworkWithID(nwid)

            if index != NSNotFound {
                networkList.removeAtIndex(index)
                tableView.reloadData()
            }
        }
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

            if network.connected {
                cell.connectedCheckbox.state = NSOnState
            }
            else {
                cell.connectedCheckbox.state = NSOffState
            }

            return cell
        }

        return nil
    }

    // end NSTableViewDelegate
}
