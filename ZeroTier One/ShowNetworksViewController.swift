//
//  ShowNetworksViewController.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/14/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class ShowNetworksViewController: NSViewController, NSTableViewDelegate, NSTableViewDataSource {

    @IBOutlet var tableView: NSTableView!

    var networkList: [Network] = [Network]()

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.

        tableView.setDelegate(self)
        tableView.setDataSource(self)
    }

    override func viewWillAppear() {
        super.viewWillAppear()

        ServiceCom.getNetworkList() { (networkList) -> Void in
            NSOperationQueue.mainQueue().addOperationWithBlock() { () -> Void in
                self.networkList = networkList
                self.tableView.reloadData()
            }
        }
    }

    // NSTableViewDataSource

    func numberOfRowsInTableView(tableView: NSTableView) -> Int {
        return networkList.count
    }

    // end NSTableViewDataSource
}
