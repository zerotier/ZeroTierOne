//
//  NetworkMonitor.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 6/16/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

let networkUpdateKey = "com.zerotier.one.network-list"

class NetworkMonitor: NSObject {

    var timer: NSTimer? = nil

    var savedNetworks: [Network] = [Network]()
    var receivedNetworks: [Network] = [Network]()
    var allNetworks: [Network] = [Network]()

    override init() {
        super.init()

        timer = NSTimer.scheduledTimerWithTimeInterval(1.0,
                                                       target: self,
                                                       selector: #selector(updateNetworkInfo),
                                                       userInfo: nil,
                                                       repeats: true)
    }

    deinit {
        timer?.invalidate()
    }

    private func  dataFile() -> String {
        var appSupport = NSFileManager.defaultManager().URLsForDirectory(.ApplicationSupportDirectory, inDomains: .UserDomainMask)[0]
        appSupport = appSupport.URLByAppendingPathComponent("ZeroTier").URLByAppendingPathComponent("One").URLByAppendingPathComponent("networks.dat")
        return appSupport.path!
    }

    func updateNetworkInfo() {
        let filePath = dataFile()

        if NSFileManager.defaultManager().fileExistsAtPath(filePath) {
            self.savedNetworks = NSKeyedUnarchiver.unarchiveObjectWithFile(filePath) as! [Network]

        }

        ServiceCom.getNetworkList() { (networkList) -> Void in
            self.receivedNetworks = networkList

            NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                self.internal_updateNetworkInfo()
            }
        }
    }

    func deleteSavedNetwork(nwid: String) {
        if let nwid = UInt64(nwid, radix: 16) {
            let index = findNetworkWithID(nwid)

            if index != NSNotFound {
                allNetworks.removeAtIndex(index)
            }
        }
    }

    // Only to be called by updateNetworkInfo()
    private func internal_updateNetworkInfo() {
        var networks = self.savedNetworks

        for nw in receivedNetworks {
            let index = findNetworkWithID(nw.nwid)

            if index != NSNotFound {
                networks[index] = nw
            }
            networks.sortInPlace({ (left, right) -> Bool in
                if left.nwid < right.nwid {
                    return true
                }

                return false
            })

            objc_sync_enter(allNetworks)
            allNetworks = networks
            objc_sync_exit(allNetworks)

            saveNetworks()

            let nc = NSNotificationCenter.defaultCenter()

            nc.postNotificationName(networkUpdateKey, object: nil, userInfo: ["networks": networks])
        }
    }

    private func findNetworkWithID(nwid: UInt64) -> Int {
        for (index, element) in allNetworks.enumerate() {

            if element.nwid == nwid {
                return index
            }
        }

        return NSNotFound
    }

    private func saveNetworks() {
        let file = dataFile()

        objc_sync_enter(allNetworks)
        NSKeyedArchiver.archiveRootObject(self.allNetworks, toFile: file)
        objc_sync_exit(allNetworks)
    }
}
