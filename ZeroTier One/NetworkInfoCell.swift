//
//  NetworkInfoCell.swift
//  ZeroTier One
//
//  Created by Grant Limberg on 5/24/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

import Cocoa

class NetworkInfoCell: NSTableCellView {

    @IBOutlet var networkIdField: NSTextField!
    @IBOutlet var networkNameField: NSTextField!

    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)

        // Drawing code here.
    }
    
}
