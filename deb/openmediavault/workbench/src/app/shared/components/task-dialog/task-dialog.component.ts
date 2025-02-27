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
 */
import {
  Component,
  ElementRef,
  EventEmitter,
  Inject,
  OnDestroy,
  OnInit,
  Output,
  ViewChild
} from '@angular/core';
import { MAT_DIALOG_DATA } from '@angular/material/dialog';
import { marker as gettext } from '@biesbjerg/ngx-translate-extract-marker';
import * as _ from 'lodash';
import { Subscription } from 'rxjs';
import { finalize, tap } from 'rxjs/operators';
import stripAnsi from 'strip-ansi';

import { Icon } from '~/app/shared/enum/icon.enum';
import { TaskDialogConfig } from '~/app/shared/models/task-dialog-config.type';
import { RpcBgResponse, RpcService } from '~/app/shared/services/rpc.service';

/**
 * A dialog with 'Start', 'Stop' and 'Cancel' buttons. It will
 * display the content received by a background task.
 */
@Component({
  selector: 'omv-task-dialog',
  templateUrl: './task-dialog.component.html',
  styleUrls: ['./task-dialog.component.scss']
})
export class TaskDialogComponent implements OnInit, OnDestroy {
  @Output()
  readonly finishEvent = new EventEmitter<string>();

  @ViewChild('content', { static: true })
  private content: ElementRef;

  // Internal
  public config: TaskDialogConfig = {} as TaskDialogConfig;
  public running = false;
  private subscription: Subscription;
  private filename: string;

  constructor(private rpcService: RpcService, @Inject(MAT_DIALOG_DATA) data: TaskDialogConfig) {
    this.config = data;
    this.sanitizeConfig();
  }

  ngOnInit(): void {
    if (this.config.startOnInit) {
      this.onStart();
    }
  }

  ngOnDestroy(): void {
    this.subscription?.unsubscribe();
  }

  onStart() {
    this.config.buttons.start.disabled = true;
    this.config.buttons.stop.disabled = false;
    this.config.buttons.close.disabled = true;
    this.config.buttons.close.dialogResult = false;
    this.content.nativeElement.innerHTML = '';
    this.running = true;
    this.subscription = this.rpcService
      .requestTaskOutput(
        this.config.request.service,
        this.config.request.method,
        this.config.request.params
      )
      .pipe(
        tap((res: RpcBgResponse) => (this.filename = res.filename)),
        finalize(() => {
          this.running = false;
          this.config.buttons.start.disabled = false;
          this.config.buttons.stop.disabled = true;
          this.config.buttons.close.disabled = false;
        })
      )
      .subscribe({
        next: (value: RpcBgResponse) => {
          const nativeEl = this.content.nativeElement;
          // Make sure we do not exceed a max. size of displayed
          // content to keep the memory consumption low.
          // Allow displaying up to 100.000 lines (80 char per line) of
          // content, then reduce to 75.000 lines in one step (smaller
          // steps may be repeated to often which is too costly).
          // Note, if your task is crossing this border, please keep in
          // mind that this dialog is not meant to be used for such
          // excessive tasks.
          if (nativeEl.innerHTML.length > 100000 * 80) {
            // Search for the first line break after the specified
            // position and cut it there if found.
            const startSearch = nativeEl.innerHTML.length - 75000 * 80;
            const pos = nativeEl.innerHTML.indexOf('\n', startSearch);
            nativeEl.innerHTML = nativeEl.innerHTML.slice(pos > 0 ? pos : startSearch);
          }
          nativeEl.innerHTML += stripAnsi(value.output);
          if (this.config.autoScroll) {
            nativeEl.scroll({ behavior: 'auto', top: nativeEl.scrollHeight });
          }
        },
        complete: () => {
          // Set the result value to `true` because the request finished
          // successfully.
          this.config.buttons.close.dialogResult = true;
          // Notify all subscribers.
          this.finishEvent.emit(this.content.nativeElement.innerHTML);
        }
      });
  }

  onStop() {
    this.subscription?.unsubscribe();
    this.rpcService
      .stopTask(this.filename)
      .pipe(
        finalize(() => {
          this.config.buttons.start.disabled = false;
          this.config.buttons.stop.disabled = true;
        })
      )
      .subscribe();
  }

  protected sanitizeConfig() {
    this.config.icon = _.get(Icon, this.config.icon, this.config.icon);
    _.defaultsDeep(this.config, {
      autoScroll: true,
      startOnInit: false,
      buttons: {
        start: {
          text: gettext('Start'),
          hidden: false,
          disabled: false,
          autofocus: false,
          class: 'omv-background-color-theme-primary'
        },
        stop: {
          text: gettext('Stop'),
          hidden: false,
          disabled: true,
          autofocus: false,
          class: 'omv-background-color-theme-red'
        },
        close: {
          text: gettext('Close'),
          hidden: false,
          disabled: false,
          autofocus: false,
          dialogResult: false
        }
      }
    });
    if (this.config.startOnInit) {
      this.config.buttons.start.hidden = true;
    }
  }
}
