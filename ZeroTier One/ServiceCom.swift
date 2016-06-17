//
//  ServiceCom.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/16/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class ServiceCom: NSObject {
    static let baseURL = "http://localhost:9993"

    private static func getKey() -> String {
        struct Holder {
            static var key: String? = nil
        }

        if Holder.key == nil {
            do {
                // Check the user's ZeroTier application support directory.  If
                // authtoken.secret exists, use it.

                var appSupportDir = try NSFileManager.defaultManager().URLForDirectory(.ApplicationSupportDirectory, inDomain: .UserDomainMask, appropriateForURL: nil, create: false)
                appSupportDir = appSupportDir.URLByAppendingPathComponent("ZeroTier")
                appSupportDir = appSupportDir.URLByAppendingPathComponent("One")
                let authtokenURL = appSupportDir.URLByAppendingPathComponent("authtoken.secret")

                if NSFileManager.defaultManager().fileExistsAtPath(authtokenURL.path!) {
                    Holder.key = try String(contentsOfURL: authtokenURL)
                }
                else {
                    try NSFileManager.defaultManager().createDirectoryAtURL(appSupportDir, withIntermediateDirectories: true, attributes: nil)

                    var authRef: AuthorizationRef = nil
                    var status = AuthorizationCreate(nil, nil, .Defaults, &authRef)

                    if status != errAuthorizationSuccess {
                        NSLog("Authorization Failed! \(status)")
                        return ""
                    }

                    var authItem = AuthorizationItem(name: kAuthorizationRightExecute, valueLength: 0, value: nil, flags: 0)
                    var authRights = AuthorizationRights(count: 1, items: &authItem)
                    let authFlags: AuthorizationFlags = [.Defaults, .InteractionAllowed, .PreAuthorize, .ExtendRights]

                    status = AuthorizationCopyRights(authRef, &authRights, nil, authFlags, nil)

                    if status != errAuthorizationSuccess {
                        NSLog("Authorization Failed! \(status)")
                        return ""
                    }

                    let localKey = getAdminAuthToken(authRef)
                    AuthorizationFree(authRef, .DestroyRights)

                    if localKey != nil && localKey.lengthOfBytesUsingEncoding(NSUTF8StringEncoding) > 0 {
                        NSLog("\(localKey)")
                        Holder.key = localKey

                        try localKey.writeToURL(authtokenURL, atomically: true, encoding: NSUTF8StringEncoding)
                    }
                }
            }
            catch {
                NSLog("Error getting app support dir: \(error)")
                Holder.key = nil
            }

        }

        if let k = Holder.key {
            return k
        }
        else {
            return ""
        }
    }

    static func getNetworkList(completionHandler: ([Network]) -> Void) {

        let urlString = baseURL + "/network?auth=\(ServiceCom.getKey())"

        let url = NSURL(string: urlString)

        if let u = url {
            let request = NSMutableURLRequest(URL: u)
            let session = NSURLSession.sharedSession()
            let task = session.dataTaskWithRequest(request) { (data, response, error) in
                if error != nil{
                    NSLog("\(error)")
                    return
                }

                let httpResponse = response as! NSHTTPURLResponse
                let status = httpResponse.statusCode

                if status == 200 {
                    do {
                        let json = try NSJSONSerialization.JSONObjectWithData(data!, options: NSJSONReadingOptions()) as! [[String: AnyObject]]

                        var networks = [Network]()
                        for jobj in json {
                            networks.append(Network(jsonData: jobj))
                        }

                        completionHandler(networks)
                    }
                    catch  {
                    }
                }
            }

            task.resume()
        }
        else {
            print("bad URL")
        }
    }


    static func joinNetwork(network: String) {
        let urlString = baseURL + "/network/\(network)?auth=\(ServiceCom.getKey())"
        let url = NSURL(string: urlString)

        if let u = url {
            let request = NSMutableURLRequest(URL: u)
            request.HTTPMethod = "POST"

            let session = NSURLSession.sharedSession()
            let task = session.dataTaskWithRequest(request) { (data, response, error) in
                let httpResponse = response as! NSHTTPURLResponse
                let status = httpResponse.statusCode

                if status == 200 {
                    print("join ok")
                }
                else {
                    print("join error: \(status)")
                }
            }

            task.resume()
        }
    }

    static func leaveNetwork(network: String) {
        let urlString = baseURL + "/network/\(network)?auth=\(ServiceCom.getKey())"
        let url = NSURL(string: urlString)

        if let u = url {
            let request = NSMutableURLRequest(URL: u)
            request.HTTPMethod = "DELETE"

            let session = NSURLSession.sharedSession()
            let task = session.dataTaskWithRequest(request) { (data, response, error) in
                let httpResponse = response as! NSHTTPURLResponse
                let status = httpResponse.statusCode

                if status == 200 {
                    print("leave ok")
                }
                else {
                    print("leave error: \(status)")
                }
            }

            task.resume()
        }
    }

}
