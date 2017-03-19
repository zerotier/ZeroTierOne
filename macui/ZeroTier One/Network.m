/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import "Network.h"

NSString *NetworkAddressesKey = @"addresses";
NSString *NetworkBridgeKey = @"bridge";
NSString *NetworkBroadcastKey = @"broadcast";
NSString *NetworkDhcpKey = @"dhcp";
NSString *NetworkMacKey = @"mac";
NSString *NetworkMtuKey = @"mtu";
NSString *NetworkMulticastKey = @"multicast";
NSString *NetworkNameKey = @"name";
NSString *NetworkNetconfKey = @"netconf";
NSString *NetworkNwidKey = @"nwid";
NSString *NetworkPortNameKey = @"port";
NSString *NetworkPortErrorKey = @"portError";
NSString *NetworkStatusKey = @"status";
NSString *NetworkTypeKey = @"type";
NSString *NetworkAllowManagedKey = @"allowManaged";
NSString *NetworkAllowGlobalKey = @"allowGlobal";
NSString *NetworkAllowDefaultKey = @"allowDefault";

@implementation Network

- (id)initWithJsonData:(NSDictionary*)jsonData
{
    self = [super init];

    if(self) {
        if([jsonData objectForKey:@"assignedAddresses"]) {
            _assignedAddresses = (NSArray<NSString*>*)[jsonData objectForKey:@"assignedAddresses"];
        }

        if([jsonData objectForKey:@"bridge"]) {
            _bridge = [(NSNumber*)[jsonData objectForKey:@"bridge"] boolValue];
        }

        if([jsonData objectForKey:@"broadcastEnabled"]) {
            _broadcastEnabled = [(NSNumber*)[jsonData objectForKey:@"broadcastEnabled"] boolValue];
        }

        if([jsonData objectForKey:@"dhcp"]) {
            _dhcp = [(NSNumber*)[jsonData objectForKey:@"dhcp"] boolValue];
        }

        if([jsonData objectForKey:@"mac"]) {
            _mac = (NSString*)[jsonData objectForKey:@"mac"];
        }

        if([jsonData objectForKey:@"mtu"]) {
            _mtu = [(NSNumber*)[jsonData objectForKey:@"mtu"] intValue];
        }

        if([jsonData objectForKey:@"name"]) {
            _name = (NSString*)[jsonData objectForKey:@"name"];
        }

        if([jsonData objectForKey:@"netconfRevision"]) {
            _netconfRevision = [(NSNumber*)[jsonData objectForKey:@"netconfRevision"] intValue];
        }

        if([jsonData objectForKey:@"nwid"]) {
            NSString *networkid = (NSString*)[jsonData objectForKey:@"nwid"];

            NSScanner *scanner = [NSScanner scannerWithString:networkid];
            [scanner scanHexLongLong:&_nwid];
        }

        if([jsonData objectForKey:@"portDeviceName"]) {
            _portDeviceName = (NSString*)[jsonData objectForKey:@"portDeviceName"];
        }

        if([jsonData objectForKey:@"portError"]) {
            _portError = [(NSNumber*)[jsonData objectForKey:@"portError"] intValue];
        }

        if([jsonData objectForKey:@"allowManaged"]) {
            _allowManaged = [(NSNumber*)[jsonData objectForKey:@"allowManaged"] boolValue];
        }

        if([jsonData objectForKey:@"allowGlobal"]) {
            _allowGlobal = [(NSNumber*)[jsonData objectForKey:@"allowGlobal"] boolValue];
        }

        if([jsonData objectForKey:@"allowDefault"]) {
            _allowDefault = [(NSNumber*)[jsonData objectForKey:@"allowDefault"] boolValue];
        }

        if([jsonData objectForKey:@"status"]) {
            NSString *statusStr = (NSString*)[jsonData objectForKey:@"status"];
            if([statusStr isEqualToString:@"REQUESTING_CONFIGURATION"]) {
                _status = REQUESTING_CONFIGURATION;
            }
            else if([statusStr isEqualToString:@"OK"]) {
                _status = OK;
            }
            else if([statusStr isEqualToString:@"ACCESS_DENIED"]) {
                _status = ACCESS_DENIED;
            }
            else if([statusStr isEqualToString:@"NOT_FOUND"]) {
                _status = NOT_FOUND;
            }
            else if([statusStr isEqualToString:@"PORT_ERROR"]) {
                _status = PORT_ERROR;
            }
            else if([statusStr isEqualToString:@"CLIENT_TOO_OLD"]) {
                _status = CLIENT_TOO_OLD;
            }
        }

        if([jsonData objectForKey:@"type"]) {
            NSString *typeStr = (NSString*)[jsonData objectForKey:@"type"];
            if([typeStr isEqualToString:@"PRIVATE"]) {
                _type = PRIVATE;
            }
            else if([typeStr isEqualToString:@"PUBLIC"]) {
                _type = PUBLIC;
            }
        }

        _connected = YES;
    }

    return self;
}
- (id)initWithCoder:(NSCoder *)aDecoder
{
    self = [super init];

    if(self) {
        if([aDecoder containsValueForKey:NetworkAddressesKey]) {
            _assignedAddresses = (NSArray<NSString*>*)[aDecoder decodeObjectForKey:NetworkAddressesKey];
        }

        if([aDecoder containsValueForKey:NetworkBridgeKey]) {
            _bridge = [aDecoder decodeBoolForKey:NetworkBridgeKey];
        }

        if([aDecoder containsValueForKey:NetworkBroadcastKey]) {
            _broadcastEnabled = [aDecoder decodeBoolForKey:NetworkBroadcastKey];
        }

        if([aDecoder containsValueForKey:NetworkDhcpKey]) {
            _dhcp = [aDecoder decodeBoolForKey:NetworkDhcpKey];
        }

        if([aDecoder containsValueForKey:NetworkMacKey]) {
            _mac = (NSString*)[aDecoder decodeObjectForKey:NetworkMacKey];
        }

        if([aDecoder containsValueForKey:NetworkMtuKey]) {
            _mtu = (int)[aDecoder decodeIntegerForKey:NetworkMtuKey];
        }

        if([aDecoder containsValueForKey:NetworkNameKey]) {
            _name = (NSString*)[aDecoder decodeObjectForKey:NetworkNameKey];
        }

        if([aDecoder containsValueForKey:NetworkNetconfKey]) {
            _netconfRevision = (int)[aDecoder decodeIntegerForKey:NetworkNetconfKey];
        }

        if([aDecoder containsValueForKey:NetworkNwidKey]) {
            _nwid = [(NSNumber*)[aDecoder decodeObjectForKey:NetworkNwidKey] unsignedLongLongValue];
        }

        if([aDecoder containsValueForKey:NetworkPortNameKey]) {
            _portDeviceName = (NSString*)[aDecoder decodeObjectForKey:NetworkPortNameKey];
        }

        if([aDecoder containsValueForKey:NetworkPortErrorKey]) {
            _portError = (int)[aDecoder decodeIntegerForKey:NetworkPortErrorKey];
        }

        if([aDecoder containsValueForKey:NetworkStatusKey]) {
            _status = (enum NetworkStatus)[aDecoder decodeIntegerForKey:NetworkStatusKey];
        }

        if([aDecoder containsValueForKey:NetworkTypeKey]) {
            _type = (enum NetworkType)[aDecoder decodeIntegerForKey:NetworkTypeKey];
        }

        if([aDecoder containsValueForKey:NetworkAllowManagedKey]) {
            _allowManaged = [aDecoder decodeBoolForKey:NetworkAllowManagedKey];
        }

        if([aDecoder containsValueForKey:NetworkAllowGlobalKey]) {
            _allowGlobal = [aDecoder decodeBoolForKey:NetworkAllowGlobalKey];
        }

        if([aDecoder containsValueForKey:NetworkAllowDefaultKey]) {
            _allowDefault = [aDecoder decodeBoolForKey:NetworkAllowDefaultKey];
        }

        _connected = NO;
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeObject:_assignedAddresses forKey:NetworkAddressesKey];
    [aCoder encodeBool:_bridge forKey:NetworkBridgeKey];
    [aCoder encodeBool:_broadcastEnabled forKey:NetworkBroadcastKey];
    [aCoder encodeBool:_dhcp forKey:NetworkDhcpKey];
    [aCoder encodeObject:_mac forKey:NetworkMacKey];
    [aCoder encodeInteger:_mtu forKey:NetworkMtuKey];
    [aCoder encodeObject:_name forKey:NetworkNameKey];
    [aCoder encodeInteger:_netconfRevision forKey:NetworkNetconfKey];
    [aCoder encodeObject:[NSNumber numberWithUnsignedLongLong:_nwid]
                  forKey:NetworkNwidKey];
    [aCoder encodeObject:_portDeviceName forKey:NetworkPortNameKey];
    [aCoder encodeInteger:_portError forKey:NetworkPortErrorKey];
    [aCoder encodeInteger:_status forKey:NetworkStatusKey];
    [aCoder encodeInteger:_type forKey:NetworkTypeKey];
    [aCoder encodeBool:_allowManaged forKey:NetworkAllowManagedKey];
    [aCoder encodeBool:_allowGlobal forKey:NetworkAllowGlobalKey];
    [aCoder encodeBool:_allowDefault forKey:NetworkAllowDefaultKey];
}

+ (BOOL)defaultRouteExists:(NSArray<Network *>*)netList
{
    for(Network *net in netList) {
        if (net.allowDefault && net.connected) {
            return YES;
        }
    }
    return NO;
}

- (NSString*)statusString {
    switch(_status) {
        case REQUESTING_CONFIGURATION:
            return @"REQUESTING_CONFIGURATION";
        case OK:
            return @"OK";
        case ACCESS_DENIED:
            return @"ACCESS_DENIED";
        case NOT_FOUND:
            return @"NOT_FOUND";
        case PORT_ERROR:
            return @"PORT_ERROR";
        case CLIENT_TOO_OLD:
            return @"CLIENT_TOO_OLD";
        default:
            return @"";
    }
}

- (NSString*)typeString {
    switch(_type) {
        case PUBLIC:
            return @"PUBLIC";
        case PRIVATE:
            return @"PRIVATE";
        default:
            return @"";
    }
}

- (BOOL)hasSameNetworkId:(UInt64)networkId
{
    return self.nwid == networkId;
}

- (BOOL)isEqualToNetwork:(Network*)network
{
    return [self.assignedAddresses isEqualToArray:network.assignedAddresses] &&
        self.bridge == network.bridge &&
        self.broadcastEnabled == network.broadcastEnabled &&
        self.dhcp == network.dhcp &&
        [self.mac isEqualToString:network.mac] &&
        self.mtu == network.mtu &&
        self.netconfRevision == network.netconfRevision &&
        [self.name isEqualToString:network.name] &&
        self.nwid == network.nwid &&
        [self.portDeviceName isEqualToString:network.portDeviceName] &&
        self.status == network.status &&
        self.type == network.type &&
        self.allowManaged == network.allowManaged &&
        self.allowGlobal == network.allowGlobal &&
        self.allowDefault == network.allowDefault &&
        self.connected == network.connected;
}

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }

    if (![object isKindOfClass:[Network class]]) {
        return NO;
    }

    return [self isEqualToNetwork:object];
}

- (NSUInteger)hash
{
    return [self.assignedAddresses hash] ^
        self.bridge ^
        self.broadcastEnabled ^
        self.dhcp ^
        [self.mac hash] ^
        self.mtu ^
        self.netconfRevision ^
        [self.name hash] ^
        self.nwid ^
        [self.portDeviceName hash] ^
        self.portError ^
        self.status ^
        self.type ^
        self.allowManaged ^
        self.allowGlobal ^
        self.allowDefault ^
        self.connected;
}

@end
