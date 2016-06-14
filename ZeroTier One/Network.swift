//
//  Network.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/17/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

enum NetworkStatus : Int, CustomStringConvertible {
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

enum NetworkType: Int, CustomStringConvertible {
    case PUBLIC
    case PRIVATE

    var description: String {
        switch self {
        case .PUBLIC: return "PUBLIC"
        case .PRIVATE: return "PRIVATE"
        }
    }
}


struct PropertyKeys {
    static let addressesKey = "addresses"
    static let bridgeKey = "bridge"
    static let broadcastKey = "broadcast"
    static let dhcpKey = "dhcp"
    static let macKey = "mac"
    static let mtuKey = "mtu"
    static let multicastKey = "multicast"
    static let nameKey = "name"
    static let netconfKey = "netconf"
    static let nwidKey = "nwid"
    static let portNameKey = "port"
    static let portErrorKey = "portError"
    static let statusKey = "status"
    static let typeKey = "type"
}

class Network: NSObject, NSCoding  {
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
    var connected: Bool = false // NOT PERSISTED.  Set to true if loaded via JSON

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

        // if it's being initialized via JSON, it's connected
        connected = true
    }

    required init?(coder aDecoder: NSCoder) {
        self.assignedAddresses = aDecoder.decodeObjectForKey(PropertyKeys.addressesKey) as! [String]
        self.bridge = aDecoder.decodeBoolForKey(PropertyKeys.bridgeKey)
        self.broadcastEnabled = aDecoder.decodeBoolForKey(PropertyKeys.broadcastKey)
        self.dhcp = aDecoder.decodeBoolForKey(PropertyKeys.dhcpKey)
        self.mac = aDecoder.decodeObjectForKey(PropertyKeys.macKey) as! String
        self.mtu = aDecoder.decodeIntegerForKey(PropertyKeys.mtuKey)
        self.multicastSubscriptions = aDecoder.decodeObjectForKey(PropertyKeys.multicastKey) as! [String]
        self.name = aDecoder.decodeObjectForKey(PropertyKeys.nameKey) as! String
        self.netconfRevision = aDecoder.decodeIntegerForKey(PropertyKeys.netconfKey)
        self.nwid = (aDecoder.decodeObjectForKey(PropertyKeys.nwidKey) as! NSNumber).unsignedLongLongValue
        self.portDeviceName = aDecoder.decodeObjectForKey(PropertyKeys.portNameKey) as! String
        self.portError = aDecoder.decodeIntegerForKey(PropertyKeys.portErrorKey)
        self.status = NetworkStatus(rawValue: aDecoder.decodeIntegerForKey(PropertyKeys.statusKey))!
        self.type = NetworkType(rawValue: aDecoder.decodeIntegerForKey(PropertyKeys.typeKey))!
    }

    func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeObject(self.assignedAddresses, forKey: PropertyKeys.addressesKey)
        aCoder.encodeBool(self.bridge, forKey: PropertyKeys.bridgeKey)
        aCoder.encodeBool(self.broadcastEnabled, forKey: PropertyKeys.broadcastKey)
        aCoder.encodeBool(self.dhcp, forKey: PropertyKeys.dhcpKey)
        aCoder.encodeObject(self.mac, forKey: PropertyKeys.macKey)
        aCoder.encodeInteger(self.mtu, forKey: PropertyKeys.mtuKey)
        aCoder.encodeObject(self.multicastSubscriptions, forKey: PropertyKeys.multicastKey)
        aCoder.encodeObject(self.name, forKey: PropertyKeys.nameKey)
        aCoder.encodeInteger(self.netconfRevision, forKey: PropertyKeys.netconfKey)
        aCoder.encodeObject(NSNumber(unsignedLongLong: self.nwid), forKey: PropertyKeys.nwidKey)
        aCoder.encodeObject(self.portDeviceName, forKey: PropertyKeys.portNameKey)
        aCoder.encodeInteger(self.portError, forKey: PropertyKeys.portErrorKey)
        aCoder.encodeInteger(self.status.rawValue, forKey: PropertyKeys.statusKey)
        aCoder.encodeInteger(self.type.rawValue, forKey: PropertyKeys.typeKey)
    }
}
