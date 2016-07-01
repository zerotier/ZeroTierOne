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
    static let allowManagedKey = "allowManaged"
    static let allowGlobalKey = "allowGlobal"
    static let allowDefaultKey = "allowDefault"
}

class Network: NSObject, NSCoding  {
    var assignedAddresses: [String] = [String]()
    var bridge: Bool = false
    var broadcastEnabled: Bool = false
    var dhcp: Bool = false
    var mac: String = ""
    var mtu: Int = 0
    var name: String = ""
    var netconfRevision: Int = 232
    var nwid: UInt64 = 0
    var portDeviceName: String = ""
    var portError: Int = 0
    var status: NetworkStatus = .REQUESTING_CONFIGURATION
    var type: NetworkType = .PRIVATE
    var allowManaged: Bool = true
    var allowGlobal: Bool = false
    var allowDefault: Bool = false
    var connected: Bool = false // NOT PERSISTED.  Set to true if loaded via JSON

    init(jsonData: [String: AnyObject]) {
        super.init()

        if let aa = jsonData["assignedAddresses"] as? [String] {
            for a in aa {
                assignedAddresses.append(a)
            }
        }

        if let b = jsonData["bridge"] as? NSNumber {
            bridge = b.boolValue
        }

        if let b = jsonData["broadcastEnabled"] as? NSNumber {
            broadcastEnabled = b.boolValue
        }

        if let d = jsonData["dhcp"] as? NSNumber {
            dhcp = d.boolValue
        }

        if let m = jsonData["mac"] as? String {
            mac = m
        }

        if let m = jsonData["mtu"] as? NSNumber {
            mtu = m.integerValue
        }

        if let n = jsonData["name"] as? String {
            name = n
        }

        if let n = jsonData["netconfRevision"] as? NSNumber {
            netconfRevision = n.integerValue
        }

        if let n = UInt64((jsonData["nwid"] as! String), radix: 16) {
            nwid = n
        }

        if let p = jsonData["portDeviceName"] as? String {
            portDeviceName = p
        }

        if let p = jsonData["portError"] as? NSNumber {
            portError = p.integerValue
        }

        if let a = jsonData["allowManaged"] as? NSNumber {
            allowManaged = a.boolValue
        }

        if let a = jsonData["allowGlobal"] as? NSNumber {
            allowGlobal = a.boolValue
        }

        if let a = jsonData["allowDefault"] as? NSNumber {
            allowDefault = a.boolValue
        }

        if let statusStr = jsonData["status"] as? String {
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
        }

        if let typeStr = jsonData["type"] as? String {
            switch typeStr {
            case "PRIVATE":
                type = .PRIVATE
            case "PUBLIC":
                type = .PUBLIC
            default:
                break
            }
        }

        // if it's being initialized via JSON, it's connected
        connected = true
    }

    required init?(coder aDecoder: NSCoder) {
        if aDecoder.containsValueForKey(PropertyKeys.addressesKey) {
            let addrs = aDecoder.decodeObjectForKey(PropertyKeys.addressesKey) as! [String]

            for a in addrs {
                self.assignedAddresses.append(a)
            }
        }

        if aDecoder.containsValueForKey(PropertyKeys.bridgeKey) {
            self.bridge = aDecoder.decodeBoolForKey(PropertyKeys.bridgeKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.broadcastKey) {
            self.broadcastEnabled = aDecoder.decodeBoolForKey(PropertyKeys.broadcastKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.dhcpKey) {
            self.dhcp = aDecoder.decodeBoolForKey(PropertyKeys.dhcpKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.macKey) {
            let mac = aDecoder.decodeObjectForKey(PropertyKeys.macKey) as! String
            self.mac = mac
        }

        if aDecoder.containsValueForKey(PropertyKeys.mtuKey) {
            self.mtu = aDecoder.decodeIntegerForKey(PropertyKeys.mtuKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.nameKey) {
            let name = aDecoder.decodeObjectForKey(PropertyKeys.nameKey) as! String

            self.name = name
        }

        if aDecoder.containsValueForKey(PropertyKeys.netconfKey) {
            self.netconfRevision = aDecoder.decodeIntegerForKey(PropertyKeys.netconfKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.nwidKey) {
            self.nwid = (aDecoder.decodeObjectForKey(PropertyKeys.nwidKey) as! NSNumber).unsignedLongLongValue
        }

        if aDecoder.containsValueForKey(PropertyKeys.portNameKey) {
            self.portDeviceName = aDecoder.decodeObjectForKey(PropertyKeys.portNameKey) as! String
        }

        if aDecoder.containsValueForKey(PropertyKeys.portErrorKey) {
            self.portError = aDecoder.decodeIntegerForKey(PropertyKeys.portErrorKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.statusKey) {
            self.status = NetworkStatus(rawValue: aDecoder.decodeIntegerForKey(PropertyKeys.statusKey))!
        }

        if aDecoder.containsValueForKey(PropertyKeys.typeKey) {
            self.type = NetworkType(rawValue: aDecoder.decodeIntegerForKey(PropertyKeys.typeKey))!
        }

        if aDecoder.containsValueForKey(PropertyKeys.allowManagedKey) {
            self.allowManaged = aDecoder.decodeBoolForKey(PropertyKeys.allowManagedKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.allowGlobalKey) {
            self.allowGlobal = aDecoder.decodeBoolForKey(PropertyKeys.allowGlobalKey)
        }

        if aDecoder.containsValueForKey(PropertyKeys.allowDefaultKey) {
            self.allowDefault = aDecoder.decodeBoolForKey(PropertyKeys.allowDefaultKey)
        }
    }

    func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeObject(self.assignedAddresses, forKey: PropertyKeys.addressesKey)
        aCoder.encodeBool(self.bridge, forKey: PropertyKeys.bridgeKey)
        aCoder.encodeBool(self.broadcastEnabled, forKey: PropertyKeys.broadcastKey)
        aCoder.encodeBool(self.dhcp, forKey: PropertyKeys.dhcpKey)
        aCoder.encodeObject(self.mac, forKey: PropertyKeys.macKey)
        aCoder.encodeInteger(self.mtu, forKey: PropertyKeys.mtuKey)
        aCoder.encodeObject(self.name, forKey: PropertyKeys.nameKey)
        aCoder.encodeInteger(self.netconfRevision, forKey: PropertyKeys.netconfKey)
        aCoder.encodeObject(NSNumber(unsignedLongLong: self.nwid), forKey: PropertyKeys.nwidKey)
        aCoder.encodeObject(self.portDeviceName, forKey: PropertyKeys.portNameKey)
        aCoder.encodeInteger(self.portError, forKey: PropertyKeys.portErrorKey)
        aCoder.encodeInteger(self.status.rawValue, forKey: PropertyKeys.statusKey)
        aCoder.encodeInteger(self.type.rawValue, forKey: PropertyKeys.typeKey)
        aCoder.encodeBool(self.allowManaged, forKey: PropertyKeys.allowManagedKey)
        aCoder.encodeBool(self.allowGlobal, forKey: PropertyKeys.allowGlobalKey)
        aCoder.encodeBool(self.allowDefault, forKey: PropertyKeys.allowDefaultKey)
    }
}

func defaultRouteExists(netList: [Network]) -> Bool {
    for net in netList {
        if net.allowDefault && net.connected {
            return true
        }
    }

    return false
}
