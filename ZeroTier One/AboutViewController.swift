//
//  AboutViewController.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 6/23/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa
import WebKit

class AboutViewController: NSViewController {


    @IBOutlet var webView: WebView!

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.
        let bundle = NSBundle.mainBundle()
        let path = bundle.URLForResource("about", withExtension: "html")

        if let url = path {
            webView.mainFrame.loadRequest(NSURLRequest(URL: url))
        }
    }
    
}
