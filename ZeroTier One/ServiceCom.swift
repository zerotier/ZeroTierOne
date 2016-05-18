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
    static var key: NSString? = "ddeb3b1e6996b6b4f2d12d10"

    static func getNetworkList(completionHandler: ([Network]) -> Void) {

        let urlString = baseURL + "/network?auth=\(ServiceCom.key!)"

        let url = NSURL(string: urlString)

        if let u = url {
            let request = NSMutableURLRequest(URL: u)
            let session = NSURLSession.sharedSession()
            let task = session.dataTaskWithRequest(request) { (data, response, error) in
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
        let urlString = baseURL + "/network/\(network)?auth=\(ServiceCom.key!)"
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
        let urlString = baseURL + "/network/\(network)?auth=\(ServiceCom.key!)"
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
