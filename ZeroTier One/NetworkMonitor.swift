//
//  NetworkMonitor.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 6/16/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

let networkUpdateKey = "com.zerotier.one.network-list"
let statusUpdateKey = "com.zerotier.one.status"

class NetworkMonitor: NSObject {

    var timer: NSTimer? = nil

    var savedNetworks: [Network] = [Network]()
    var receivedNetworks: [Network] = [Network]()
    var allNetworks: [Network] = [Network]()

    override init() {
        super.init()
    }

    deinit {
        timer?.invalidate()
    }

    func start() {
        NSLog("ZeroTier monitor started")
        timer = NSTimer.scheduledTimerWithTimeInterval(1.0,
                                                       target: self,
                                                       selector: #selector(updateNetworkInfo),
                                                       userInfo: nil,
                                                       repeats: true)
    }


    func stop() {
        NSLog("ZeroTier monitor stopped")
        timer?.invalidate()
        timer = nil
    }

    private func  dataFile() -> String {
        var appSupport = NSFileManager.defaultManager().URLsForDirectory(.ApplicationSupportDirectory, inDomains: .UserDomainMask)[0]
        appSupport = appSupport.URLByAppendingPathComponent("ZeroTier").URLByAppendingPathComponent("One").URLByAppendingPathComponent("networks.dat")
        return appSupport.path!
    }

    func updateNetworkInfo() {
        let filePath = dataFile()

        if NSFileManager.defaultManager().fileExistsAtPath(filePath) {
            let networks = NSKeyedUnarchiver.unarchiveObjectWithFile(filePath) as! [Network]

            self.savedNetworks.removeAll()

            for n in networks {
                self.savedNetworks.append(n)
            }
        }

        ServiceCom.getNetworkList() { (networkList) -> Void in
            self.receivedNetworks = networkList

            NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                self.internal_updateNetworkInfo()
            }
        }

        ServiceCom.getNodeStatus() { nodeStatus -> Void in
            NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                let nc = NSNotificationCenter.defaultCenter()

                nc.postNotificationName(statusUpdateKey, object: nil, userInfo: ["status": nodeStatus])
            }
        }
    }

    func deleteSavedNetwork(nwid: String) {
        if let nwid = UInt64(nwid, radix: 16) {
            let index = findNetworkWithID(nwid)

            if index != NSNotFound {
                allNetworks.removeAtIndex(index)
            }

            let index2 = findSavedNetworkWithID(nwid)

            if index2 != NSNotFound {
                savedNetworks.removeAtIndex(index2)
            }
        }

        saveNetworks()
    }

    // Only to be called by updateNetworkInfo()
    private func internal_updateNetworkInfo() {
        var networks = self.savedNetworks

        for nw in receivedNetworks {
            let index = findSavedNetworkWithID(nw.nwid)

            if index != NSNotFound {
                networks[index] = nw
            }
            else {
                networks.append(nw)
            }
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

    private func findNetworkWithID(nwid: UInt64) -> Int {
        for (index, element) in allNetworks.enumerate() {

            if element.nwid == nwid {
                return index
            }
        }

        return NSNotFound
    }

    private func findSavedNetworkWithID(nwid: UInt64) -> Int {
        for (index, element) in savedNetworks.enumerate() {

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
