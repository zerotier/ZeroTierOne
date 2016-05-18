//
//  Network.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/17/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

enum NetworkStatus : CustomStringConvertible {
    case REQUESTING_CONFIGURATION
    case OK
    case ACCESS_DENIED
    case NOT_FOUND
    case PORT_ERROR
    case CLIENT_TOO_OLD

    var description: String {
        switch self {
        case .REQUESTING_CONFIGURATION: return "REQUESTING_CONFIGURATION"
        case .OK: return "OK"
        case .ACCESS_DENIED: return "ACCESS_DENIED"
        case .NOT_FOUND: return "NOT_FOUND"
        case .PORT_ERROR: return "PORT_ERROR"
        case .CLIENT_TOO_OLD: return "CLIENT_TOO_OLD"
        }
    }
}

enum NetworkType: CustomStringConvertible {
    case PUBLIC
    case PRIVATE

    var description: String {
        switch self {
        case .PUBLIC: return "PUBLIC"
        case .PRIVATE: return "PRIVATE"
        }
    }
}

class Network: NSObject {
    var assignedAddresses: [String] = [String]()
    var bridge: Bool = false
    var broadcastEnabled: Bool = false
    var dhcp: Bool = false
    var mac: String = ""
    var mtu: Int = 0
    var multicastSubscriptions: [String] = [String]()
    var name: String = ""
    var netconfRevision: Int = 232
    var nwid: UInt64 = 0
    var portDeviceName: String = ""
    var portError: Int = 0
    var status: NetworkStatus = .REQUESTING_CONFIGURATION
    var type: NetworkType = .PRIVATE

    init(jsonData: [String: AnyObject]) {
        super.init()

        let aa = jsonData["assignedAddresses"] as! [String]
        for a in aa {
            assignedAddresses.append(a)
        }

        bridge = (jsonData["bridge"] as! NSNumber).boolValue
        broadcastEnabled = (jsonData["broadcastEnabled"] as! NSNumber).boolValue
        dhcp = (jsonData["dhcp"] as! NSNumber).boolValue
        mac = jsonData["mac"] as! String
        mtu = (jsonData["mtu"] as! NSNumber).integerValue

        let multSubs = jsonData["multicastSubscriptions"] as! [String]
        for ms in multSubs {
            multicastSubscriptions.append(ms)
        }

        name = jsonData["name"] as! String
        netconfRevision = (jsonData["netconfRevision"] as! NSNumber).integerValue
        nwid = UInt64((jsonData["nwid"] as! String), radix: 16)!
        portDeviceName = jsonData["portDeviceName"] as! String
        portError = (jsonData["portError"] as! NSNumber).integerValue

        let statusStr = jsonData["status"] as! String
        switch statusStr {
        case "REQUESTING_CONFIGURATION":
            status = .REQUESTING_CONFIGURATION
        case "OK":
            status = .OK
        case "ACCESS_DENIED":
            status = .ACCESS_DENIED
        case "NOT_FOUND":
            status = .NOT_FOUND
        case "PORT_ERROR":
            status = .PORT_ERROR
        case "CLIENT_TOO_OLD":
            status = .CLIENT_TOO_OLD
        default:
            break
        }

        let typeStr = jsonData["type"] as! String
        switch typeStr {
        case "PRIVATE":
            type = .PRIVATE
        case "PUBLIC":
            type = .PUBLIC
        default:
            break
        }
    }
}
