import { Injectable } from '@angular/core';

import * as _ from 'lodash';
import { ToastsManager } from 'ng2-toastr';
import { BehaviorSubject } from 'rxjs';

import { NotificationType } from '../enum/notification-type.enum';
import { CdNotification } from '../models/cd-notification';
import { FinishedTask } from '../models/finished-task';
import { ServicesModule } from './services.module';
import { TaskMessageService } from './task-message.service';

@Injectable({
  providedIn: ServicesModule
})
export class NotificationService {
  // Observable sources
  private dataSource = new BehaviorSubject<CdNotification[]>([]);

  // Observable streams
  data$ = this.dataSource.asObservable();

  KEY = 'cdNotifications';

  constructor(public toastr: ToastsManager, private taskMessageService: TaskMessageService) {
    const stringNotifications = localStorage.getItem(this.KEY);
    let notifications: CdNotification[] = [];

    if (_.isString(stringNotifications)) {
      notifications = JSON.parse(stringNotifications, (key, value) => {
        if (_.isPlainObject(value)) {
          return _.assign(new CdNotification(), value);
        }
        return value;
      });
    }

    this.dataSource.next(notifications);
  }

  /**
   * Removes all current saved notifications
   */
  removeAll() {
    localStorage.removeItem(this.KEY);
    this.dataSource.next([]);
  }

  /**
   * Method used for saving a shown notification (check show() method).
   * @param {Notification} notification
   */
  save(type: NotificationType, title: string, message?: string) {
    const notification = new CdNotification(type, title, message);

    const recent = this.dataSource.getValue();
    recent.push(notification);
    while (recent.length > 10) {
      recent.shift();
    }

    this.dataSource.next(recent);
    localStorage.setItem(this.KEY, JSON.stringify(recent));
  }

  /**
   * Method for showing a notification.
   * @param {NotificationType} type toastr type
   * @param {string} title
   * @param {string} [message] The message to be displayed. Note, use this field
   *   for error notifications only.
   * @param {*} [options] toastr compatible options, used when creating a toastr
   * @memberof NotificationService
   * @returns The timeout ID that is set to be able to cancel the notification.
   */
  show(type: NotificationType, title: string, message?: string, options?: any) {
    return setTimeout(() => {
      this.save(type, title, message);
      if (!message) {
        message = '';
      }
      switch (type) {
        case NotificationType.error:
          this.toastr.error(message, title, options);
          break;
        case NotificationType.info:
          this.toastr.info(message, title, options);
          break;
        case NotificationType.success:
          this.toastr.success(message, title, options);
          break;
      }
    }, 10);
  }

  notifyTask(finishedTask: FinishedTask, success: boolean = true) {
    if (finishedTask.success && success) {
      this.show(NotificationType.success, this.taskMessageService.getSuccessTitle(finishedTask));
    } else {
      this.show(
        NotificationType.error,
        this.taskMessageService.getErrorTitle(finishedTask),
        this.taskMessageService.getErrorMessage(finishedTask)
      );
    }
  }

  /**
   * Prevent the notification from being shown.
   * @param {number} timeoutId A number representing the ID of the timeout to be canceled.
   */
  cancel(timeoutId) {
    clearTimeout(timeoutId);
  }
}
