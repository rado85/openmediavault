<?php
/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @copyright Copyright (c) 2009-2022 Volker Theile
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */
namespace Engined\Rpc;

class Config extends \OMV\Rpc\ServiceAbstract {
	/**
	 * Get the RPC service name.
	 */
	public function getName() {
		return "Config";
	}

	/**
	 * Initialize the RPC service.
	 */
	public function initialize() {
		$this->registerMethod("isDirty");
		$this->registerMethod("applyChanges");
		$this->registerMethod("applyChangesBg");
		$this->registerMethod("revertChanges");
		$this->registerMethod("revertChangesBg");
	}

	/**
	 * Check if the configuration has been modified.
	 * @return TRUE if the configuration is modified and the changes have
	 *   not been applied until now, otherwise FALSE.
	 */
	public function isDirty($params, $context) {
		// Validate the RPC caller context.
		$this->validateMethodContext($context, [
			"role" => OMV_ROLE_ADMINISTRATOR
		]);
		$moduleMngr = \OMV\Engine\Module\Manager::getInstance();
		$modules = $moduleMngr->getDirtyModules();
		return !empty($modules);
	}

	/**
	 * Apply the configuration changes.
	 * The following notifications will be sent for each processed module:<ul>
	 * \li org.openmediavault.module.service.<module>.start
	 * \li org.openmediavault.module.service.<module>.stop
	 * \li org.openmediavault.module.service.<module>.applyconfig
	 * </ul>
	 * @param params An array containing the following fields:
	 *   \em modules The list of modules that should be applied. If empty
	 *   all dirty modules will be processed.
	 *   \em force Set to TRUE to update all modules, regardless if marked
	 *   as dirty or not.
	 * @param context The context of the caller.
	 * @return void
	 * @throw \OMV\Exception
	 */
	public function applyChanges($params, $context) {
		// Validate the RPC caller context.
		$this->validateMethodContext($context, [
			"role" => OMV_ROLE_ADMINISTRATOR
		]);
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, "rpc.config.applychanges");
		$moduleMngr = \OMV\Engine\Module\Manager::getInstance();
		$notifyDispatcher = \OMV\Engine\Notify\Dispatcher::getInstance();
		// Open the dirty modules file. Lock it to force all writers
		// to wait until this RPC has been finished.
		$jsonFile = new \OMV\Json\File(\OMV\Environment::get(
		  "OMV_ENGINED_DIRTY_MODULES_FILE"));
		$jsonFile->open("c+");
		// Make sure we have a valid JSON file. Keep in mind that the file
		// is empty if it was created by the \OMV\Json\File::open method.
		if ($jsonFile->isEmpty())
			$jsonFile->write([]);
		// Read the file content.
		$dirtyModules = $jsonFile->read();
		// Remove non-existing modules from list. It may happen that the
		// list contains entries from already deinstalled plugins.
		$removeDirtyModules = [];
		$modules = array_keys($moduleMngr->getModules());
		foreach ($dirtyModules as $dirtyModulek => $dirtyModulev) {
			if (!in_array($dirtyModulev, $modules))
				$removeDirtyModules[] = $dirtyModulev;
		}
		if (!empty($removeDirtyModules)) {
			// Remove module name from list of dirty modules. Note, the
			// array must be re-indexed.
			$dirtyModules = array_values(array_diff($dirtyModules,
				$removeDirtyModules));
		}
		// Get list of modules to be processed. If the parameter 'modules'
		// is empty, then process all registered modules.
		$modules = $params['modules'];
		if (empty($params['modules']))
			$modules = array_keys($moduleMngr->getModules());
		$processModules = [];
		foreach ($modules as $modulek => $modulev) {
			// Skip module if 'force' is disabled and module is not marked
			// as dirty.
			if (!in_array($modulev, $dirtyModules) &&
				(FALSE === $params['force'])) {
					continue;
				}
			// Get the module object and apply the configuration.
			if (is_null($inst = $moduleMngr->getModule($modulev))) {
				throw new \OMV\Exception("Module '%s' not found.",
					$modulev);
			}
			// Append module to list of modules that have to be processed.
			$processModules[mb_strtolower($modulev)] = $inst;
			// Remove module name from list of dirty modules. Note, the
			// array must be re-indexed.
			$dirtyModules = array_values(array_diff($dirtyModules,
				[$modulev]));
		}
		// Build the dependency list and deploy the configuration for the
		// specified modules.
		$tsort = new \OMV\Util\TopologicalSort();
		$tsort->clean();
		foreach ($processModules as $name => $inst) {
			// Add the modules that need to be executed before the
			// given module.
			$afterNames = $inst->deployAfter();
			if (FALSE === $tsort->add($name, $afterNames)) {
				throw new \OMV\Exception(
					"Failed to add node (name=%s, dependencies=[%s]).",
					$name, implode(",", $afterNames));
			}
			// Add the modules that need to be executed after the
			// given module.
			$beforeNames = $inst->deployBefore();
			foreach ($beforeNames as $beforeNamek => $beforeNamev) {
				if (FALSE === $tsort->add($beforeNamev, [$name])) {
					throw new \OMV\Exception(
						"Failed to add node (name=%s, dependencies=[%s]).",
						$beforeNamev, $name);
				}
			}
		}
		foreach ($tsort->sort() as $name) {
			// Skip modules that are not marked as dirty.
			if (!array_key_exists($name, $processModules))
				continue;
			$this->debug("Deploying configuration for module '%s'", $name);
			$inst = $processModules[$name];
			$uri = "org.openmediavault.module.service.%s.%s";
			$inst->preDeploy();
			$notifyDispatcher->notify(OMV_NOTIFY_EVENT,
				sprintf($uri, $name, "predeploy"));
			$inst->deploy();
			$notifyDispatcher->notify(OMV_NOTIFY_EVENT,
				sprintf($uri, $name, "deploy"));
			$inst->postDeploy();
			$notifyDispatcher->notify(OMV_NOTIFY_EVENT,
				sprintf($uri, $name, "postdeploy"));
		}
		// Reset list of dirty modules.
		$jsonFile->write($dirtyModules);
		$jsonFile->close();
		// Unlink all configuration revision files.
		$db = \OMV\Config\Database::getInstance();
		$db->unlinkRevisions();
	}

	/**
	 * Apply the configuration changes in a background process.
	 * @param params The method parameters. \see applyChanges.
	 * @param context The context of the caller.
	 * @return The name of the background process status file.
	 */
	public function applyChangesBg($params, $context) {
		return $this->callMethodBg("applyChanges", $params, $context);
	}

	/**
	 * Revert the configuration changes.
	 * @param params An array containing the following fields:
	 *   \em filename The revision file. Set to empty to automatically detect
	 *   the correct revision file.
	 * @param context The context of the caller.
	 * @return void
	 * @throw \OMV\Exception
	 */
	public function revertChanges($params, $context) {
		// Validate the RPC caller context.
		$this->validateMethodContext($context, [
			"role" => OMV_ROLE_ADMINISTRATOR
		]);
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, "rpc.config.revertchanges");
		// Open the dirty modules file. Lock it to force all writers
		// to wait until this RPC has been finished.
		$jsonFile = new \OMV\Json\File(\OMV\Environment::get(
		  "OMV_ENGINED_DIRTY_MODULES_FILE"));
		$jsonFile->open("c+");
		// Revert configuration changes. All existing revision files
		// will be deleted.
		$db = \OMV\Config\Database::getInstance();
		$db->revert($params['filename']);
		// Empty the dirty modules file.
		$jsonFile->write(array());
		$jsonFile->close();
	}

	/**
	 * Revert the configuration changes in a background process.
	 * @param params The method parameters. \see revertChanges.
	 * @param context The context of the caller.
	 * @return The name of the background process status file.
	 */
	public function revertChangesBg($params, $context) {
		return $this->callMethodBg("revertChanges", $params, $context);
	}
}
