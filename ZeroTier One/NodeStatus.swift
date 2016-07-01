//
//  NodeStatus.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 6/30/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class NodeStatus: NSObject {
    var address: String = ""
    var publicIdentity: String = ""
    var online: Bool = false
    var tcpFallbackActive: Bool = false
    var versionMajor: Int = 0
    var versionMinor: Int = 0
    var versionRev: Int = 0
    var version: String = ""
    var clock: UInt64 = 0

    init(jsonData: [String: AnyObject]) {
        if let a = jsonData["address"] as? String {
            self.address = a
        }

        if let p = jsonData["publicIdentity"] as? String {
            self.publicIdentity = p
        }

        if let o = jsonData["online"] as? NSNumber {
            self.online = o.boolValue
        }

        if let t = jsonData["tcpFallbackActive"] as? NSNumber {
            self.tcpFallbackActive = t.boolValue
        }

        if let v = jsonData["versionMajor"] as? NSNumber {
            self.versionMajor = v.integerValue
        }

        if let v = jsonData["versionMinor"] as? NSNumber {
            self.versionMinor = v.integerValue
        }

        if let v = jsonData["versionRev"] as? NSNumber {
            self.versionRev = v.integerValue
        }

        if let v = jsonData["version"] as? String {
            self.version = v
        }

        if let c = jsonData["version"] as? NSNumber {
            self.clock = c.unsignedLongLongValue
        }
    }
}
