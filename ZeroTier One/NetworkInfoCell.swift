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

    @IBOutlet var statusField: NSTextField!
    @IBOutlet var typeField: NSTextField!
    @IBOutlet var macField: NSTextField!
    @IBOutlet var mtuField: NSTextField!
    @IBOutlet var broadcastField: NSTextField!
    @IBOutlet var bridgingField: NSTextField!
    @IBOutlet var deviceField: NSTextField!
    @IBOutlet var addressesField: NSTextField!

    override func awakeFromNib() {
        super.awakeFromNib()
        
    }
    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)

        // Drawing code here.
    }
    
}
