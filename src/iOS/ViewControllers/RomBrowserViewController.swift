//
//  RomBrowserViewController.swift
//  Nintendoish
//
//  Created by Ingebretsen, Andrew (HBO) on 6/13/18.
//  Copyright © 2018 Ingebretsen, Andrew (HBO). All rights reserved.
//

import UIKit
import CoreData

class RomBrowserViewController: UITableViewController, UIDocumentPickerDelegate, NSFetchedResultsControllerDelegate {
    
    lazy var persistentContainer: GameLibraryPersistentContainer = {
        let container = GameLibraryPersistentContainer(name: "GameLibrary")
        
        container.loadPersistentStores(completionHandler: { (storeDescription, error) in
            if let error = error {
                fatalError("Unresolved error \(error)")
            }
        })
        return container
    }()
    
    var fetchedResultsController:NSFetchedResultsController<Rom>!
    var hasRoms:Bool {
        get {
            guard let count = fetchedResultsController.sections?[0].numberOfObjects else {
                return false
            }
            return count > 0
        }
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        tableView.estimatedRowHeight = 0
        tableView.estimatedSectionFooterHeight = 0
        tableView.estimatedSectionHeaderHeight = 0
        
        let fr:NSFetchRequest<Rom> = Rom.fetchRequest()
        fr.sortDescriptors = [NSSortDescriptor(keyPath: \Rom.game.name, ascending: true)]
        fetchedResultsController = NSFetchedResultsController(fetchRequest: fr, managedObjectContext: persistentContainer.viewContext, sectionNameKeyPath: nil, cacheName: nil)
        fetchedResultsController.delegate = self
        try? fetchedResultsController.performFetch()
    }

    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        let rom:Rom = sender as! Rom
        let vc:NESViewController = segue.destination as! NESViewController
        vc.rom = rom
        vc.title = rom.game.strippedName
    }
    
    override func tableView(_ tableView: UITableView, trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath) -> UISwipeActionsConfiguration? {
        if (hasRoms == false) {
            return UISwipeActionsConfiguration(actions: [])
        }
        
        let deleteAction = UIContextualAction.init(style: .destructive, title: "Remove", handler: {_,_,completionHandler in
            do {
                self.persistentContainer.viewContext.delete(self.fetchedResultsController.object(at: indexPath))
                try self.persistentContainer.viewContext.save()
                completionHandler(true)
            } catch {
                completionHandler(false)
                print("Error deleting object")
            }
        })
        
        return UISwipeActionsConfiguration(actions: [deleteAction])
    }
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        if (hasRoms == false) {
            return
        }
        tableView.deselectRow(at: indexPath, animated: true)
        let rom:Rom = self.fetchedResultsController.object(at: indexPath)
        performSegue(withIdentifier: "playRom", sender: rom)
    }

    override func numberOfSections(in tableView: UITableView) -> Int {
        let count = fetchedResultsController.sections?.count
        return count!
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        guard let count = fetchedResultsController.sections?[section].numberOfObjects else {
            return 1
        }
        return max(count, 1)
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        guard let count = fetchedResultsController.sections?[indexPath.section].numberOfObjects else {
            return tableView.dequeueReusableCell(withIdentifier: "NoRomCell", for: indexPath)
        }
        if (count == 0) {
            return tableView.dequeueReusableCell(withIdentifier: "NoRomCell", for: indexPath)
        }
        
        let rom:Rom = self.fetchedResultsController.object(at: indexPath)
        let cell = tableView.dequeueReusableCell(withIdentifier: "RomCell", for: indexPath) as! RomBrowserTableViewCell

        cell.titleLabel.text = rom.game.strippedName
        cell.coverImage.image = UIImage(data: rom.game.boxImage! as Data)

        return cell
    }

    @IBAction func addButton(_ sender: Any) {
        let vc:UIDocumentPickerViewController = UIDocumentPickerViewController.init(documentTypes: ["nintendo.nes"], in: .import)
        vc.delegate = self
        vc.allowsMultipleSelection = true
        present(vc, animated: true, completion: nil)
    }
    
    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        print("Processing \(urls.count) files")
        var successfulImports = 0
        var failedImports = 0
        let context = self.persistentContainer.viewContext
        do {
            for url in urls {
                // Get the md5 hash
                let romData = try Data(contentsOf: url)
                let md5:String = romData.advanced(by: 16).MD5().uppercased()
                
                //Find game
                let fetchRequest:NSFetchRequest<Game> = Game.fetchRequest()
                fetchRequest.predicate = NSPredicate(format: "md5 = %@", md5)
                fetchRequest.fetchLimit = 1
                if let game = try context.fetch(fetchRequest).first {
                    if game.rom != nil {
                        // game all ready exists
                        print("Game all ready exists")
                        failedImports += 1
                    } else {
                        //Create the rom object
                        let romObj:Rom = NSEntityDescription.insertNewObject(forEntityName: "Rom", into: context) as! Rom
                        romObj.game = game
                        romObj.romData = romData as NSData
                        print("Created Rom")
                        
                        // Save
                        try context.save()
                        successfulImports += 1
                    }
                } else {
                    //Didn't find the game
                    print("Couldn't find md5 has that matches:" + md5)
                    failedImports += 1
                }
            }
            if failedImports != 0 {
                self.displayMessage("Failed to import \(failedImports) games.\n\n Either the rom all ready exists in your library, or you tried to import a game that is unrecognized in our rom database.")
            }
        } catch {
            print("Error creating entity:\(error)")
        }
    }
    
    func displayMessage(_ message:String) {
        let alertController = UIAlertController(title: "Notice", message: message, preferredStyle: .alert)
        alertController.addAction(UIAlertAction(title: "Ok", style: .default, handler: nil))
        self.present(alertController, animated: true, completion: nil)
    }
    
    func controllerWillChangeContent(_ controller: NSFetchedResultsController<NSFetchRequestResult>) {
        tableView.beginUpdates()
    }
    
    func controller(_ controller: NSFetchedResultsController<NSFetchRequestResult>, didChange anObject: Any, at indexPath: IndexPath?, for type: NSFetchedResultsChangeType, newIndexPath: IndexPath?) {
        if type == .insert, let index = newIndexPath {
            self.tableView.insertRows(at: [index], with: .automatic)
        } else if type == .delete, let index = indexPath {
            self.tableView.deleteRows(at: [index], with: .automatic)
        }
    }
    
    func controllerDidChangeContent(_ controller: NSFetchedResultsController<NSFetchRequestResult>) {
        tableView.endUpdates()
    }
}
