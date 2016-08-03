//
//  ShowNetworksViewController.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/14/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class ShowNetworksViewController: NSViewController, NSTableViewDelegate, NSTableViewDataSource {

    @IBOutlet var tableView: NSTableView?

    var networkList: [Network] = [Network]()
    var netMonitor: NetworkMonitor!

    var visible = false

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.

        tableView?.setDelegate(self)
        tableView?.setDataSource(self)
        tableView?.backgroundColor = NSColor.clearColor()
    }

    private func  dataFile() -> String {
        var appSupport = NSFileManager.defaultManager().URLsForDirectory(.ApplicationSupportDirectory, inDomains: .UserDomainMask)[0]
        appSupport = appSupport.URLByAppendingPathComponent("ZeroTier").URLByAppendingPathComponent("One").URLByAppendingPathComponent("networks.dat")
        return appSupport.path!
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        visible = true
    }

    override func viewWillDisappear() {
        super.viewWillDisappear()

        let filePath = dataFile()
        NSKeyedArchiver.archiveRootObject(self.networkList, toFile: filePath)
        visible = false
    }

    func deleteNetworkFromList(nwid: String) {
        netMonitor.deleteSavedNetwork(nwid)
    }


    func setNetworks(list: [Network]) {
        networkList = list
        if visible {
            tableView?.reloadData()
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

            if network.connected {
                cell.connectedCheckbox.state = NSOnState

                if network.allowDefault {
                    cell.allowDefault.enabled = true
                    cell.allowDefault.state = NSOnState
                }
                else {
                    cell.allowDefault.state = NSOffState

                    if defaultRouteExists(networkList) {
                        cell.allowDefault.enabled = false
                    }
                    else {
                        cell.allowDefault.enabled = true
                    }
                }

                cell.allowGlobal.enabled = true
                cell.allowManaged.enabled = true
            }
            else {
                cell.connectedCheckbox.state = NSOffState

                cell.allowDefault.enabled = false
                cell.allowGlobal.enabled = false
                cell.allowManaged.enabled = false
            }
            
            cell.allowGlobal.state = (network.allowGlobal ? NSOnState : NSOffState)
            cell.allowManaged.state = (network.allowManaged ? NSOnState : NSOffState)

            cell.addressesField.stringValue = ""

            for nw in network.assignedAddresses {
                cell.addressesField.stringValue += "\(nw)\n"
            }



            return cell
        }

        return nil
    }

    // end NSTableViewDelegate
}
